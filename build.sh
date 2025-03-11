
micromamba deactivate
micromamba activate pfr-env

if [[ "$1" == "test" ]]; then
	scons uninstall & scons clean && scons build && scons test && scons install
else
	scons uninstall & scons clean && scons build && scons install
fi