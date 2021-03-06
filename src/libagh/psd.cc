// ;-*-C++-*-
/*
 *       File name:  libagh/psd.cc
 *         Project:  Aghermann
 *          Author:  Andrei Zavada <johnhommer@gmail.com>
 *                   Parts from PhysioToolKit (http://www.physionet.org/physiotools,
 *                   by George B. Moody (george@mit.edu))
 * Initial version:  2010-04-28
 *
 *         Purpose:  CBinnedPower methods
 *
 *         License:  GPL
 */


#include <sys/stat.h>
#include <fcntl.h>

#include <cassert>

#include <omp.h>
#include <fftw3.h>

#include "misc.hh"

#include "psd.hh"
#include "edf.hh"

#if HAVE_CONFIG_H
#  include "config.h"
#endif

using namespace std;



// true if all ok;
// false, and assign defaults, if values are totally looney;
// still false, and clamp bin_size to satisfy constraints if necessary
bool
agh::SFFTParamSet::validate()
{
	if ( page_size < 0 || page_size > 120 ||
	     freq_trunc < 4. || freq_trunc > 80. ||
	     (TWinType_underlying_type)welch_window_type > (TWinType_underlying_type)TWinType::_total ) {
		assign_defaults();
		return false;
	}
	return true;
}






// must match those defined in glade
const array<const char*, 8>
	agh::SFFTParamSet::welch_window_type_names = {{
	"Bartlett", "Blackman", "Blackman-Harris",
	"Hamming",  "Hanning",  "Parzen",
	"Square",   "Welch"
}};





// The following window functions have been taken from fft.c, part of WFDB package

#define TWOPI (M_PI*2)

/* See Oppenheim & Schafer, Digital Signal Processing, p. 241 (1st ed.) */
double
win_bartlett( size_t j, size_t n)
{
	double a = 2.0/(n-1), w;
	if ( (w = j*a) > 1. )
		w = 2. - w;
	return w;
}

/* See Oppenheim & Schafer, Digital Signal Processing, p. 242 (1st ed.) */
double
win_blackman( size_t j, size_t n)
{
	double a = TWOPI/(n-1), w;
	w = 0.42 - .5 * cos(a * j) + .08 * cos(2 * a * j);
	return w;
}

/* See Harris, F.J., "On the use of windows for harmonic analysis with the
   discrete Fourier transform", Proc. IEEE, Jan. 1978 */
double
win_blackman_harris( size_t j, size_t n)
{
	double a = TWOPI/(n-1), w;
	w = 0.35875 - 0.48829 * cos(a * j) + 0.14128 * cos(2 * a * j) - 0.01168 * cos(3 * a * j);
	return w;
}

/* See Oppenheim & Schafer, Digital Signal Processing, p. 242 (1st ed.) */
double
win_hamming( size_t j, size_t n)
{
	double a = TWOPI/(n-1), w;
	w = 0.54 - 0.46*cos(a*j);
	return w;
}

/* See Oppenheim & Schafer, Digital Signal Processing, p. 242 (1st ed.)
   The second edition of Numerical Recipes calls this the "Hann" window. */
double
win_hanning( size_t j, size_t n)
{
	double a = TWOPI/(n-1), w;
	w = 0.5 - 0.5*cos(a*j);
	return w;
}

/* See Press, Flannery, Teukolsky, & Vetterling, Numerical Recipes in C,
   p. 442 (1st ed.) */
double
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
double
win_square( size_t j, size_t n)
{
	return 1.0;
}

/* See Press, Flannery, Teukolsky, & Vetterling, Numerical Recipes in C,
   p. 442 (1st ed.) or p. 554 (2nd ed.) */
double
win_welch( size_t j, size_t n)
{
	double a = (n-1)/2.0, w;
	w = (j-a)/(a+1);
	w = 1 - w*w;
	return w;
}



double (*agh::winf[])(size_t, size_t) = {
	win_bartlett,
	win_blackman,
	win_blackman_harris,
	win_hamming,
	win_hanning,
	win_parzen,
	win_square,
	win_welch
};





list<pair<float,float>>
agh::CBinnedPower::artifacts()
{
	auto &src = _using_F->signals[_using_sig_no].artifacts;
	list<pair<float,float> > ret (src.size());
	auto A = src.begin();
	auto B = ret.begin();
	while ( A != src.end() ) {
		(B++)->first  = (A++)->first  / (float)samplerate;
		(B++)->second = (A++)->second / (float)samplerate;
	}
	return ret;
}



string
agh::CBinnedPower::fname_base() const
{
	UNIQUE_CHARP (_);
	assert (asprintf( &_,
			  "%s-%s-%zu-%g-%c%c-%zu",
			  source().filename(), source()[sig_no()].channel.c_str(), page_size, freq_trunc,
			  'a'+(char)welch_window_type, 'a'+(char)_using_F->signals[_using_sig_no].af_dampen_window_type,
			  _signature) > 1);
	string ret {_};
	return ret;
}



// static int
// _find_latest_mirror( const char* mask, string& recp)
// {
// 	glob_t g;
// 	glob( mask, 0, NULL, &g);

// 	if ( g.gl_pathc == 0 )
// 		return -1;

// 	size_t latest_entry = 0;
// 	if ( g.gl_pathc > 1 ) {
// 		struct stat t;
// 		time_t latest = (time_t)0;
// 		for ( size_t i = 0; i < g.gl_pathc; ++i ) {
// 			if ( stat( g.gl_pathv[i], &t) )
// 				return -1;
// 			if ( t.st_mtime > latest ) {
// 				latest = t.st_mtime;
// 				latest_entry = i;
// 			}
// 		}
// 	}

// 	recp.assign( g.gl_pathv[latest_entry]);
// 	globfree( &g);

// 	return 0;
// }



int
agh::CBinnedPower::obtain_power( const CEDFFile& F, int sig_no,
				 const SFFTParamSet& req_params)
{
      // check if we have it already
	size_t req_signature = F[sig_no].dirty_signature();
	if ( _data.size() > 0 && (*this) == req_params
	     && _signature == req_signature )
		return 0;

      // remember source and channel for reuse in obtain_power()
	_using_F = &F;
	_using_sig_no = sig_no;

	samplerate = F.samplerate( sig_no);
	size_t	spp      = samplerate * page_size,
		pages    = floor((float)F.length() / page_size);
	double	freq_max = (double)(spp+1)/2 / samplerate;
	size_t	bins     = freq_max / bin_size;
	//printf( "pages == F.CHypnogram::length() ? %zu == %zu\npagesize = %zu, spp = %zu; bin_size = %g\n", pages, F.CHypnogram::length(), page_size, spp, bin_size);
	assert (pages == F.CHypnogram::length());
	resize( pages);
	fprintf( stderr, "CBinnedPower::obtain_power( %s, %s): %zu sec (%zu sec per CBinnedPower), %zu pages; bins/size/freq_max = %zu/%g/%g\n",
		 F.filename(), F[sig_no].channel.c_str(), F.length(), length_in_seconds(), pages, bins, bin_size, freq_max);
	// fprintf( stderr, "bin_size = %g, page_size = %zu; %zu bins\n",
	// 	 bin_size, page_size, n_bins());

	UNIQUE_CHARP (old_mirror_fname);
	UNIQUE_CHARP (new_mirror_fname);

	// insert a .
	string basename_dot = string (F.filename());
	basename_dot.insert( basename_dot.rfind( '/') + 1, ".");

	assert (asprintf( &old_mirror_fname,
			  "%s-%s-%zu-%g-%c%c-%zu.power",
			  basename_dot.c_str(),
			  F[sig_no].channel.c_str(), page_size, bin_size,
			  'a'+(char)welch_window_type, 'a'+(char)F.signals[sig_no].af_dampen_window_type,
			  _signature) > 1);

      // update signature
	*(SFFTParamSet*)this = req_params;
	_signature = req_signature;
	assert (asprintf( &new_mirror_fname,
			  "%s-%s-%zu-%g-%c%c-%zu.power",
			  basename_dot.c_str(),
			  F[sig_no].channel.c_str(), page_size, bin_size,
			  'a'+(char)welch_window_type, 'a'+(char)F[sig_no].af_dampen_window_type,
			  _signature) > 1);

	bool got_it = (_mirror_back( new_mirror_fname) == 0);

//	printf( "%s\n%s\n\n", old_mirror_fname, new_mirror_fname);
      // remove previously saved power
	if ( strcmp( old_mirror_fname, new_mirror_fname) )
		if ( unlink( old_mirror_fname) )
			;

	if ( got_it )
		return 0;

      // 0. get signal sample, truncate to n_pages
	valarray<double> S = F.get_signal_filtered<size_t, double>( sig_no);
	if ( S.size() == 0 )
		return -1;

      // 1. dampen samples marked as artifacts
	// already done in get_signal_filtered()

      // 2. zero-mean and detrend
	// zero-mean already done in CEDFFile::get_signal_filtered()

      // 3. apply windowing function
	{
	      // (a) create a static vector of multipliers
		valarray<double>
			W (spp);
		for ( size_t i = 0; i < spp; ++i )
			W[i] = winf[(size_t)welch_window_type]( i, spp);

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
	static size_t saved_spp = 0;

	if ( fft_plan == NULL || spp != saved_spp ) {
		n_procs = omp_get_max_threads();

		fprintf( stderr, "Will use %d core(s)\nPreparing fftw plan for %zu samples...", n_procs, spp);
		saved_spp = spp;

		for_each( fft_Ti.begin(), fft_Ti.end(), fftw_free);
		for_each( fft_To.begin(), fft_To.end(), fftw_free);

		fft_Ti.resize( n_procs);
		fft_To.resize( n_procs);
		P.resize( n_procs);

		auto Ii = fft_Ti.begin(), Io = fft_To.begin();
		auto Ip = P.begin();
		for ( ; Ii != fft_Ti.end(); ++Ii, ++Io, ++Ip ) {
			*Ii = (double*) fftw_malloc( sizeof(double) * spp * 2);
			*Io = (double*) fftw_malloc( sizeof(double) * spp * 2);
			Ip->resize( spp+2);
		}
		// and let them lie spare

		memcpy( fft_Ti[0], &S[0], spp * sizeof(double));  // not necessary?
		fft_plan = fftw_plan_dft_r2c_1d( spp, &fft_Ti[0][0], (fftw_complex*)&fft_To[0][0], 0 /* FFTW_PATIENT */);
		fprintf( stderr, "done\n");
	}

	// go
	int ThId;
	double	f = 0.;
	size_t	p, b, k = 1;
	size_t	chunk = pages/n_procs + 2;
#pragma omp parallel for schedule(dynamic, chunk), private(ThId, b, f, p)
	for ( p = 0; p < pages; ++p ) {
		ThId = omp_get_thread_num();
		memcpy( &fft_Ti[ThId][0], &S[p*spp], spp * sizeof(double));

		fftw_execute_dft_r2c( fft_plan, &fft_Ti[ThId][0], (fftw_complex*)&fft_To[ThId][0]);

	      // thanks http://www.fftw.org/fftw2_doc/fftw_2.html
		P[ThId][0] = fft_To[ThId][0] * fft_To[ThId][0];		/* DC component */
		for ( k = 1; k < (spp+1)/2; ++k )		/* (k < N/2 rounded up) */
			P[ThId][k] = fft_To[ThId][k    ] * fft_To[ThId][k    ]
				   + fft_To[ThId][spp-k] * fft_To[ThId][spp-k];
		if ( unlikely (spp % 2 == 0) )			/* N is even */
			P[ThId][spp/2] = fft_To[ThId][spp/2] * fft_To[ThId][spp/2];	/* Nyquist freq. */

	      // 5. collect power into bins
		// the frequency resolution in P is (1/samplerate) Hz, right?
		////memcpy( &_data[p*bins], &P[ThId][0], bins * sizeof(double));
		///printf( "n_bins = %zu, max_freq = %g\n", n_bins(), max_freq);
		for ( f = 0., b = 0; b < bins; (f += bin_size), ++b ) {
			//printf( "b = %zu, f = %g\n", b, f);
			nmth_bin(p, b) =
				valarray<double>
				(P[ThId][ slice( f*samplerate, (f+bin_size)*samplerate, 1) ]) . sum();
		}
		/// / (bin_size * samplerate) // don't; power is cumulative
	}

	if ( _mirror_enable( new_mirror_fname) )
		;

	return 0;
}




int
agh::CBinnedPower::_mirror_enable( const char *fname)
{
	int fd, retval = 0;
	if ( (fd = open( fname, O_RDWR | O_CREAT | O_TRUNC, 0644)) == -1 ||
	     write( fd, &_data[0], _data.size() * sizeof(double)) == -1 )
	     retval = -1;

	close( fd);
	return retval;
}


int
agh::CBinnedPower::_mirror_back( const char *fname)
{
	int fd = -1;
	try {
		if ( (fd = open( fname, O_RDONLY)) == -1 )
			throw -1;
		if ( read( fd, &_data[0], _data.size() * sizeof(double))
		     != (ssize_t)(_data.size() * sizeof(double)) )
			throw -2;
//		fprintf( stderr, "CBinnedPower::_mirror_back(\"%s\") ok\n", fname);
		return 0;
	} catch (int ex) {
//		fprintf( stderr, "CBinnedPower::_mirror_back(\"%s\") failed\n", fname);
		if ( fd != -1 ) {
			close( fd);
			if ( unlink( fname) )
				;
		}
		return ex;
	}
}






int
agh::CBinnedPower::export_tsv( const string& fname)
{
	FILE *f = fopen( fname.c_str(), "w");
	if ( !f )
		return -1;

	obtain_power();

	size_t bin, p;
	float bum = 0.;

	const CEDFFile &F = source();
	char *asctime_ = asctime( localtime( &F.start_time));
	fprintf( f, "## Subject: %s;  Session: %s, Episode: %s recorded %.*s;  Channel: %s\n"
		 "## Total spectral power course (%zu %zu-sec pages) up to %g Hz in bins of %g Hz\n"
		 "#Page\t",
		 F.patient.c_str(), F.session.c_str(), F.episode.c_str(),
		 (int)strlen(asctime_)-1, asctime_,
		 F[sig_no()].channel.c_str(),
		 n_pages(), pagesize(), n_bins()*bin_size, bin_size);

	for ( bin = 0; bin < n_bins(); ++bin, bum += bin_size )
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
agh::CBinnedPower::export_tsv( float from, float upto,
			       const string& fname)
{
	FILE *f = fopen( fname.c_str(), "w");
	if ( !f )
		return -1;

	const CEDFFile &F = source();
	char *asctime_ = asctime( localtime( &F.start_time));
	fprintf( f, "## Subject: %s;  Session: %s, Episode: %s recorded %.*s;  Channel: %s\n"
		 "## Spectral power course (%zu %zu-sec pages) in range %g-%g Hz\n",
		 F.patient.c_str(), F.session.c_str(), F.episode.c_str(),
		 (int)strlen(asctime_)-1, asctime_,
		 F[sig_no()].channel.c_str(),
		 n_pages(), pagesize(), from, upto);

	valarray<double> course = power_course<double>( from, upto);
	for ( size_t p = 0; p < n_pages(); ++p )
		fprintf( f, "%zu\t%g\n", p, course[p]);

	fclose( f);
	return 0;
}



// EOF
