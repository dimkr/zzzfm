EXTRA_DIST = zzzfm.conf

install-data-hook:
	test -d $(DESTDIR)/$(sysconfdir)/zzzfm || mkdir -p -- \
				$(DESTDIR)/$(sysconfdir)/zzzfm
	test -f $(DESTDIR)/$(sysconfdir)/zzzfm/zzzfm.conf || $(INSTALL_DATA) \
				$(srcdir)/zzzfm.conf \
				$(DESTDIR)/$(sysconfdir)/zzzfm/zzzfm.conf

