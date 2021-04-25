During package installation, an /etc/xdg/zzzfm/ directory is created and
is populated with this "session" file and "scripts" directory (empty).

These enable the packager (or distro curator, or local sysadmin) to provide
a customized set of "factory default" settings to be applied first-run for
each user (and to be re-applied in the event the user chooses to delete
their ~/.config/zzzfm/ and start afresh).

A curator does not need to manually edit this "default" session file (and
probably should not do so). Instead, launch and run the program as a normal user,
adjust the various settings, preferences, layout, then exit the program...
and copy that (your) user's session file into place (here, within the project
source tree).

NOTE:
skidoo is shipping a session file which has "geany" as the user's selected editor
and "leafpad" (as the user's selected AsRoot editor). Also, sudo and gksu are
preselected as the auth mechanisms.
