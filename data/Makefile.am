#pkgdata_DATA = ui/idle-bg.svg

applicationsdir = $(datadir)/applications
applications_DATA = aghermann.desktop edfhed-gtk.desktop
uidir = $(datadir)/${PACKAGE}/ui
ui_DATA = ui/agh-ui-main.glade ui/agh-ui-sf.glade ui/agh-ui-mf.glade ui/edf-header-editor.glade \
	ui/idle-bg.svg ui/aghermann.png ui/equations1.png

EXTRA_DIST = ui/agh-ui-main.glade ui/agh-ui-sf.glade ui/agh-ui-mf.glade ui/edf-header-editor.glade \
	ui/idle-bg.svg ui/aghermann.png ui/equations1.png \
	aghermann.desktop edfhed-gtk.desktop


update_desktop_database = update-desktop-database $(datadir)/applications

install-data-hook:
	@sed -e "s/_VERSION_/$(VERSION)/" -i $(uidir)/*.glade;	\
	if test -z "$(DESTDIR)"; then					\
		$(mkinstalldirs) $(uidir); 				\
		$(mkinstalldirs) $(datadir)/applications;               \
		echo "Updating desktop database:";			\
		$(update_desktop_database) $(DESTDIR);		\
	else								\
		echo "*** Desktop database not updated. Remember to run:";\
		echo "***";						\
		echo "***   $(update_desktop_database)";		\
		echo "***";						\
	fi
