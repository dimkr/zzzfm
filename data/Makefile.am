desktopdir = $(datadir)/applications
dt_in_files = zzzfm.desktop.in \
			zzzfm-find.desktop.in \
			zzzfm-folder-handler.desktop.in
desktop_DATA = $(dt_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

DISTCLEANFILES = zzzfm.desktop \
			zzzfm-find.desktop \
			zzzfm-folder-handler.desktop


icondir = $(datadir)/pixmaps
icon_DATA = \
			zzzfm.png \
			zzzfm-root.png \
			zzzfm24.png \
			zzzfm-root24.png \
			zzzfm-find.png \
			zzzfm-128-cube-blue.png \
			zzzfm-128-cube-red.png \
			zzzfm-48-cube-blue.png \
			zzzfm-48-cube-red.png \
			zzzfm-48-folder-blue.png \
			zzzfm-48-folder-red.png


filedir = $(datadir)/zzzfm
uidir=$(filedir)/ui


# html-doc
htmldir = @htmldir@
# howdy
html_DATA = zzzfm-manual-en.html


# GtkBuilder UI definition files    howdy  FYI    THE .glade FILES ARE NOT PRESENT IN THE SOURCEPACKAGE
ui_in_files= \
	ui/prefdlg.glade \
	ui/prefdlg2.glade \
	ui/appchooserdlg.glade \
	ui/file_properties.glade \
	ui/find-files.glade \
	ui/find-files2.glade \
	ui/godlg.glade
ui_DATA = $(ui_in_files:.glade=.ui)

EXTRA_DIST = \
	$(desktop_DATA) \
	$(icon_DATA) \
	$(file_DATA) \
	$(ui_DATA)

CLEANFILES = zzzfm.desktop \
		 zzzfm-find.desktop \
		 zzzfm-folder-handler.desktop

# Generate GtkBuilder UI files from Glade files
# was with GNU extension: %.ui: %.glade
.glade.ui:
#	$(GTK_BUILDER_CONVERT) $< $@
	$(top_srcdir)/gtk-builder-convert $< $@
#	$(top_builddir)/src/xml-purge $@

