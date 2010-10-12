// ;-*-C++-*- *  Time-stamp: "2010-10-05 23:14:19 hmmr"

/*
 * Author: Andrei Zavada (johnhommer@gmail.com)
 *
 * License: GPL
 *
 * Initial version: 2010-04-28
 */


#include <sys/stat.h>
#include <glob.h>
#include <fcntl.h>

#include <ctime>
#include <cassert>
#include <cmath>
#include <memory>

#include <omp.h>
#include <fftw3.h>

#include "misc.hh"

#include "psd.hh"
#include "edf.hh"


using namespace std;





#define TWOPI (M_PI*2)

/* See Oppenheim & Schafer, Digital Signal Processing, p. 241 (1st ed.) */
static double
win_bartlett( size_t j, size_t n)
{
	double a = 2.0/(n-1), w;
	if ( (w = j*a) > 1. )
		w = 2. - w;
	return w;
}

/* See Oppenheim & Schafer, Digital Signal Processing, p. 242 (1st ed.) */
static double
win_blackman( size_t j, size_t n)
{
	double a = TWOPI/(n-1), w;
	w = 0.42 - .5 * cos(a * j) + .08 * cos(2 * a * j);
	return w;
}

/* See Harris, F.J., "On the use of windows for harmonic analysis with the
   discrete Fourier transform", Proc. IEEE, Jan. 1978 */
static double
win_blackman_harris( size_t j, size_t n)
{
	double a = TWOPI/(n-1), w;
	w = 0.35875 - 0.48829 * cos(a * j) + 0.14128 * cos(2 * a * j) - 0.01168 * cos(3 * a * j);
	return w;
}

/* See Oppenheim & Schafer, Digital Signal Processing, p. 242 (1st ed.) */
static double
win_hamming( size_t j, size_t n)
{
	double a = TWOPI/(n-1), w;
	w = 0.54 - 0.46*cos(a*j);
	return w;
}

/* See Oppenheim & Schafer, Digital Signal Processing, p. 242 (1st ed.)
   The second edition of Numerical Recipes calls this the "Hann" window. */
static double
win_hanning( size_t j, size_t n)
{
	double a = TWOPI/(n-1), w;
	w = 0.5 - 0.5*cos(a*j);
	return w;
}

/* See Press, Flannery, Teukolsky, & Vetterling, Numerical Recipes in C,
   p. 442 (1st ed.) */
static double
win_parzen( size_t j, size_t n)
{
	double a = (n-1)/2.0, w;
	if ( (w = (j-a)/(a+1)) > 0.0 )
		w = 1 - w;
	else
		w = 1 + w;
	return w;
}

/* See any of the above references. */
static double
win_square( size_t j, size_t n)
{
	return 1.0;
}

/* See Press, Flannery, Teukolsky, & Vetterling, Numerical Recipes in C,
   p. 442 (1st ed.) or p. 554 (2nd ed.) */
static double
win_welch( size_t j, size_t n)
{
	double a = (n-1)/2.0, w;
	w = (j-a)/(a+1);
	w = 1 - w*w;
	return w;
}


typedef double (*d_f_zz)(size_t, size_t);

static d_f_zz winf[] = {
	win_bartlett,
	win_blackman,
	win_blackman_harris,
	win_hamming,
	win_hanning,
	win_parzen,
	win_square,
	win_welch
};





string
CBinnedPower::fname_base()
{
	UNIQUE_CHARP (_);
	assert (asprintf( &_,
			  "%s-%s-%zu-%g-%c%c-%zu",
			  source().filename(), source()[sig_no()].Channel.c_str(), page_size, bin_size,
			  'a'+welch_window_type, 'a'+af_dampen_window_type,
			  hash<const char*>()(artifacts())) > 1);
	return string (_);
}



static int
_find_latest_mirror( const char* mask, string& recp)
{
	glob_t g;
	glob( mask, 0, NULL, &g);

	if ( g.gl_pathc == 0 )
		return -1;

	size_t latest_entry = 0;
	if ( g.gl_pathc > 1 ) {
		struct stat t;
		time_t latest = (time_t)0;
		for ( size_t i = 0; i < g.gl_pathc; ++i ) {
			if ( stat( g.gl_pathv[i], &t) )
				return -1;
			if ( t.st_mtime > latest ) {
				latest = t.st_mtime;
				latest_entry = i;
			}
		}
	}

	recp.assign( g.gl_pathv[latest_entry]);
	globfree( &g);

	return 0;
}



int
CBinnedPower::obtain_power( const CEDFFile& F, int sig_no,
			    const SFFTParamSet& req_params,
			    const char* req_artifacts)
{
      // check if we have it already
	if ( _data.size() > 0 && is_equal( req_params)
	     && (req_artifacts && _artifacts == req_artifacts) )
		return 0;

      // remember source and channel for use in import_artifacts
	_using_F = &F;
	_using_sig_no = sig_no;

	samplerate = F[sig_no].SamplesPerRecord / F.DataRecordSize;
	size_t	spp = samplerate * page_size;
	size_t	pages = floor((float)F.length() / page_size);
	resize( pages);
	// printf( "%zu sec (%zu sec per CBinnedPower), bin_size = %g, page_size = %zu; %zu pages, %zu bins\n",
	// 	F.length(), length_in_seconds(), bin_size, page_size, pages, n_bins());

      // expressly check if a saved .power exists (first entry)
	if ( req_artifacts == NULL ) {
		UNIQUE_CHARP (any_mirror_fname);
		assert (asprintf( &any_mirror_fname, "%s-%s-%zu-%g-%c%c-*.power",
				  F.filename(), F[sig_no].Channel.c_str(),
				  page_size, bin_size, 'a'+welch_window_type, 'a'+af_dampen_window_type) > 1 );

		string	latest_mirror_fname;
		if ( _find_latest_mirror( any_mirror_fname, latest_mirror_fname) == 0 )
			if ( _mirror_back( latest_mirror_fname.c_str() ) == 0 )
			     return 0;
	}


	UNIQUE_CHARP (old_mirror_fname);
	UNIQUE_CHARP (new_mirror_fname);

	assert (asprintf( &old_mirror_fname,
			  "%s-%s-%zu-%g-%c%c-%zu.power",
			  F.filename(), F[sig_no].Channel.c_str(), page_size, bin_size,
			  'a'+welch_window_type, 'a'+af_dampen_window_type,
			  HASHKEY(artifacts())) > 1);

      // (re)assign SFFTParamSet
	assign( req_params);
	assert (asprintf( &new_mirror_fname,
			  "%s-%s-%zu-%g-%c%c-%zu.power",
			  F.filename(), F[sig_no].Channel.c_str(), page_size, bin_size,
			  'a'+welch_window_type, 'a'+af_dampen_window_type,
			  req_artifacts ? HASHKEY(req_artifacts) : 0) > 1);

	if ( _mirror_back( new_mirror_fname) == 0 )
		return 0;

      // remove previously saved power
	if ( req_artifacts && _artifacts == req_artifacts )  // meaning the user just changed some window_type: eligible for keeping around
		;
	else
		if ( unlink( old_mirror_fname) )
			;

      // 0. get signal sample
	valarray<double> S;
	if ( F.get_signal_data( sig_no, 0, F.NDataRecords, S) )
		return -1;

      // 1. dampen samples marked as artifacts
	if ( req_artifacts ) {
		for ( size_t sa = 0; sa < strlen( req_artifacts); ++sa )
			if ( req_artifacts[sa] == 'x' ) {
				// find a contiguous artifact run
				size_t sz = sa + 1;
				while ( sz < strlen( req_artifacts) && req_artifacts[sz] == 'x' )
					++sz;
//				printf("x at %zu,%zu", sa, sz);

				valarray<double>
					W (samplerate * (sz - sa));

			      // construct a vector of multipliers using an INVERTED windowing function on the
			      // first and last seconds of the run
				size_t	t, t0;
				for ( t = 0; t < samplerate/2; ++t )
					W[t] = (1 - winf[af_dampen_window_type]( t, samplerate));
				t0 = (sz-sa-1) * samplerate;  // start of the last page but one
				for ( t = samplerate/2; t < samplerate; ++t )
					W[t0 + t] = (1 - winf[af_dampen_window_type]( t, samplerate));
			      // AND, connect mid-first to mid-last seconds (at lowest value of the window)
				W[ slice(samplerate/2, (sz-sa-1)*samplerate, 1) ] = (1 - winf[af_dampen_window_type]( samplerate/2, samplerate));
//				printf( "  lowest = %g\n", 1 - winf[af_dampen_window_type]( samplerate/2, samplerate));
			      // now gently apply the multiplier vector onto the artifacts
				S[ slice(sa*samplerate, (sz-sa)*samplerate, 1) ] *= W;

				sa = sz;
			}
		_artifacts.assign( req_artifacts);
	}


      // 2. zero-mean and detrend
	// don't waste time: it's EEG!

      // 3. apply windowing function
	{
	      // (a) create a static vector of multipliers
		valarray<double>
			W (spp);
		for ( size_t i = 0; i < spp; ++i )
			W[i] = winf[welch_window_type]( i, spp);

	      // (b) apply it page by page
		for ( size_t p = 0; p < pages; ++p )
			S[ slice(p * spp, 1 * spp, 1) ] *= W;
	}
      // 4. obtain power spectrum
	// prepare

	static vector<double*>	fft_Ti;
	static vector<double*>	fft_To;
	static int n_procs = 1;
	static vector<valarray<double>>	// buffer for PSD
		P;
	static fftw_plan fft_plan = NULL;


	if ( fft_plan == NULL ) {
		n_procs = omp_get_max_threads();
		fprintf( stderr, "Will use %d core(s)\nPreparing fftw plan...", n_procs);

		fft_Ti.resize( n_procs);
		fft_To.resize( n_procs);
		P.resize( n_procs);

		auto Ii = fft_Ti.begin(), Io = fft_To.begin(), Ip = P.begin();
		for ( ; Ii != fft_Ti.end(); ++Ii, ++Io, ++Ip ) {
			*Ii = (double*)fftw_malloc( sizeof(double) * spp * 2);
			*Io = (double*)fftw_malloc( sizeof(double) * spp * 2);
			Ip->resize( spp/2+1);
		}
		// and let them lie spare

		// if ( fft_plan )
		// 	fftw_destroy_plan( fft_plan);
		memcpy( fft_Ti[0], &S[0], spp * sizeof(double));  // not necessary?
		fft_plan = fftw_plan_dft_r2c_1d( spp, &fft_Ti[0][0], (fftw_complex*)&fft_To[0][0], 0 /* FFTW_PATIENT */);
		fprintf( stderr, "done\n");
	}

	// go
	int ThId;
//#pragma omp parallel for schedule(static, (pages/n_procs)), private(ThId)
	size_t chunk = pages/n_procs + 1;
#pragma omp parallel for schedule(static, chunk), private(ThId)
	for ( size_t p = 0; p < pages; ++p ) {
		ThId = omp_get_thread_num();
		memcpy( &fft_Ti[ThId][0], &S[p*spp], spp * sizeof(double));

		fftw_execute_dft_r2c( fft_plan, &fft_Ti[ThId][0], (fftw_complex*)&fft_To[ThId][0]);

	      // thanks http://www.fftw.org/fftw2_doc/fftw_2.html
		P[ThId][0] = fft_To[ThId][0] * fft_To[ThId][0];		/* DC component */
		for ( size_t k = 1; k < (spp+1)/2; ++k )		/* (k < N/2 rounded up) */
			P[ThId][k] = fft_To[ThId][k    ] * fft_To[ThId][k    ]
				   + fft_To[ThId][spp-k] * fft_To[ThId][spp-k];
		if ( spp % 2 == 0 )			/* N is even */
			P[ThId][spp/2] = fft_To[ThId][spp/2] * fft_To[ThId][spp/2];	/* Nyquist freq. */

	      // 5. collect power into bins
		// the frequency resolution in P is (1/samplerate) Hz, right?
		float	max_freq = spp/samplerate,
			f;
		size_t	b;
		for ( f = 0., b = 0; f < max_freq; (f += bin_size), ++b )
			nmth_bin(p, b) =
				valarray<double> (P[ThId][ slice( f*samplerate, (f+bin_size)*samplerate, 1) ]) . sum();
		// / (bin_size * samplerate) // don't; power is cumulative
	}

	if ( _mirror_enable( new_mirror_fname) )
		;

	return 0;
}




int
CBinnedPower::_mirror_enable( const char *fname)
{
	fprintf( stderr, "enable mirror %s\n", fname);
	int fd, retval = 0;
	if ( (fd = open( fname, O_RDWR | O_CREAT | O_TRUNC, 0644)) == -1 ||
	     write( fd, &_data[0], _data.size() * sizeof(double)) == -1 ||
	     write( fd, _artifacts.c_str(), _artifacts.size()) == -1 )
	     retval = -1;

	close( fd);
	return retval;
}


int
CBinnedPower::_mirror_back( const char *fname)
{
	int fd = -1;
	try {
		if ( (fd = open( fname, O_RDONLY)) == -1 )
			throw -1;
		if ( read( fd, &_data[0], _data.size() * sizeof(double))
		     != (ssize_t)(_data.size() * sizeof(double)) )
			throw -2;
		if ( read( fd, &_artifacts[0], _artifacts.size() * sizeof(char))
		     != (ssize_t)(_artifacts.size() * sizeof(char)) )
			throw -3;
		fprintf( stderr, "CBinnedPower::_mirror_back(\"%s\") ok\n", fname);
		return 0;
	} catch (int ex) {
		fprintf( stderr, "CBinnedPower::_mirror_back(\"%s\") failed\n", fname);
		if ( fd != -1 ) {
			close( fd);
			if ( unlink( fname) )
				;
		}
		return ex;
	}
}






int
CBinnedPower::export_tsv( const char* fname) const
{
	FILE *f = fopen( fname, "w");
	if ( !f )
		return -1;

	size_t bin, p;
	float bum = 0.;

	const CEDFFile &F = source();
	char *asctime_ = asctime( &F.timestamp_struct);
	fprintf( f, "## Subject: %s;  Session: %s, Episode: %s recorded %.*s;  Channel: %s\n"
		 "## Total spectral power course (%zu %zu-sec pages) up to %g Hz in bins of %g Hz\n"
		 "#Page\t",
		 F.PatientID_raw, F.Session.c_str(), F.Episode.c_str(),
		 (int)strlen(asctime_)-1, asctime_,
		 F[sig_no()].Channel.c_str(),
		 n_pages(), pagesize(), n_bins()*binsize(), binsize());

	for ( bin = 0; bin < n_bins(); ++bin, bum += binsize() )
		fprintf( f, "%g%c", bum, bin+1 == n_bins() ? '\n' : '\t');

	for ( p = 0; p < n_pages(); ++p ) {
		fprintf( f, "%zu", p);
		for ( bin = 0; bin < n_bins(); bin++ )
			fprintf( f, "\t%g", nmth_bin( p, bin));
		fprintf( f, "\n");
	}

	fclose( f);
	return 0;
}




int
CBinnedPower::export_tsv( float from, float upto,
			  const char* fname) const
{
	FILE *f = fopen( fname, "w");
	if ( !f )
		return -1;

	const CEDFFile &F = source();
	char *asctime_ = asctime( &F.timestamp_struct);
	fprintf( f, "## Subject: %s;  Session: %s, Episode: %s recorded %.*s;  Channel: %s\n"
		 "## Spectral power course (%zu %zu-sec pages) in range %g-%g Hz\n",
		 F.PatientID_raw, F.Session.c_str(), F.Episode.c_str(),
		 (int)strlen(asctime_)-1, asctime_,
		 F[sig_no()].Channel.c_str(),
		 n_pages(), pagesize(), from, upto);

	valarray<double> course = power_course( from, upto);
	for ( size_t p = 0; p < n_pages(); ++p )
		fprintf( f, "%zu\t%g\n", p, course[p]);

	fclose( f);
	return 0;
}



// EOF
