lexd.dat: root.txt en.txt
	genrb -d . root.txt en.txt
	echo root.res en.res > package_list.txt
	pkgdata -p lexd --mode archive -d . package_list.txt

localesdir = $(LOCALES_DIR)
dist_locales_DATA = lexd.dat
