## Process this file with automake to produce Makefile.in

SUBDIRS = src po

tsciconsdir = $(datadir)/tsclient/icons
icon_sizes = 16 24 32 48 64 96 128 192 256 512 1024

desktopdir = $(datadir)/applications
desktop_in_files = tsclient.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

applicationsdir = $(datadir)/application-registry
applications_DATA = tsclient.applications

mimedir = $(datadir)/mime-info
mime_DATA = tsclient.mime tsclient.keys

man1_MANS = tsclient.1
man1dir = $(datadir)/man/man1

intlfiles = intltool-extract.in intltool-merge.in intltool-update.in

CLEANFILES = \
	tsclient.desktop \
	intltool-extract \
	intltool-merge \
	intltool-update \
	$(NULL)

EXTRA_DIST = \
	autogen.sh \
	$(desktop_in_files) \
	$(applications_DATA)	\
	$(mime_DATA)	\
	$(man1_MANS)	\
	$(intlfiles)	\
	tsclient.spec \
	icons

install-data-local:
	@$(NORMAL_INSTALL)
	if test -d $(srcdir)/icons; then \
	  $(mkinstalldirs) $(DESTDIR)$(tsciconsdir); \
	  for icon in $(srcdir)/icons/*; do \
	    if test -f $$icon; then \
	      $(INSTALL_DATA) $$icon $(DESTDIR)$(tsciconsdir); \
	    fi \
	  done; \
	  for size in $(icon_sizes); do \
	    base=tsclient_$${size}x$${size}.png; \
	    src=$(srcdir)/icons/$$base; \
	    if test -f $$src; then \
	      dir=$(DESTDIR)$(datadir)/icons/hicolor/$${size}x$${size}/apps; \
	      $(mkinstalldirs) $$dir; \
	      $(INSTALL_DATA) $$src $$dir/tsclient.png; \
	    fi \
	  done; \
	  if test -f $(srcdir)/icons/tsclient.svg; then \
	    dir=$(DESTDIR)$(datadir)/icons/hicolor/scalable/apps; \
	    $(mkinstalldirs) $$dir; \
	    $(INSTALL_DATA) $(srcdir)/icons/tsclient.svg $$dir/tsclient.svg; \
	  fi \
	fi

uninstall-local:
	@$(NORMAL_UNINSTALL)
	rm -rf $(DESTDIR)$(tsciconsdir)
	for size in $(icon_sizes); do \
	  dir=$(DESTDIR)$(datadir)/icons/hicolor/$${size}x$${size}/apps; \
	  rm -f $$dir/tsclient.png; \
	done
	rm -f $(DESTDIR)$(datadir)/icons/hicolor/scalable/apps/tsclient.svg

dist-hook:
	if test -d icons; then \
	  mkdir -p $(distdir)/icons; \
	  for icon in icons/*; do \
	    if test -f $$icon; then \
	      cp -p $$icon $(distdir)/icons; \
	    fi \
	  done; \
	fi

install-man1:
	$(mkinstalldirs) $(DESTDIR)$(man1dir)
	@list='$(man1_MANS)'; \
	l2='$(man_MANS)'; for i in $$l2; do \
	  case "$$i" in \
	    *.1*) list="$$list $$i" ;; \
	  esac; \
	done; \
	for i in $$list; do \
	  if test -f $(srcdir)/$$i; then file=$(srcdir)/$$i; \
	  else file=$$i; fi; \
	  ext=`echo $$i | sed -e 's/^.*\\.//'`; \
	  inst=`echo $$i | sed -e 's/\\.[0-9a-z]*$$//'`; \
	  inst=`echo $$inst | sed '$(transform)'`.$$ext; \
	  echo " $(INSTALL_DATA) $$file $(DESTDIR)$(man1dir)/$$inst"; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(man1dir)/$$inst; \
	done

uninstall-man1:
	@list='$(man1_MANS)'; \
	l2='$(man_MANS)'; for i in $$l2; do \
	  case "$$i" in \
	    *.1*) list="$$list $$i" ;; \
	  esac; \
	done; \
	for i in $$list; do \
	  ext=`echo $$i | sed -e 's/^.*\\.//'`; \
	  inst=`echo $$i | sed -e 's/\\.[0-9a-z]*$$//'`; \
	  inst=`echo $$inst | sed '$(transform)'`.$$ext; \
	  echo " rm -f $(DESTDIR)$(man1dir)/$$inst"; \
	  rm -f $(DESTDIR)$(man1dir)/$$inst; \
	done
install-man: $(MANS)
	@$(NORMAL_INSTALL)
	$(MAKE) $(AM_MAKEFLAGS) install-man1
uninstall-man:
	@$(NORMAL_UNINSTALL)
	$(MAKE) $(AM_MAKEFLAGS) uninstall-man1
