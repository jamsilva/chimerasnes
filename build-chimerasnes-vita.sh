#!/bin/bash

setup()
{
	rm -rf libretro-chimerasnes chimerasnes chimerasnes.zip
	git clone "https://github.com/jamsilva/chimerasnes" libretro-chimerasnes
}

build_vita()
{
	pushd retroarch
	git reset --hard HEAD
	git clean -f -x -d
	popd

	pushd libretro-chimerasnes
	git reset --hard HEAD
	git clean -f -x -d
	popd

	export HAVE_VITAGLES=$2
	./libretro-build-vita.sh chimerasnes
	SINGLE_CORE=chimerasnes FORCE=YES NOCLEAN=1 EXIT_ON_ERROR=1 ./libretro-buildbot-recipe.sh recipes/playstation/vita
	mkdir -p "chimerasnes/$1/info"
	cp retroarch/pkg/vita/retroarch.vpk/vpk/chimerasnes_libretro.self "chimerasnes/$1"
	cp retroarch/pkg/vita/retroarch.vpk/vpk/info/chimerasnes_libretro.info "chimerasnes/$1/info"
}

if [ "$VITASDK" == "" ]; then
	export VITASDK="/opt/vitasdk/"
fi

export PATH="$VITASDK/bin:$PATH"

setup
build_vita "piglet" 1
build_vita "vita2d" 0
cat << EOF > "chimerasnes/howto_install.txt"
Copy chimerasnes_libretro.self from the appropriate folder (piglet or vita2d; if you don't know which to use, go for vita2d first) to /partition:/app/RETROVITA (where partition is usually ux0; this folder should exist, you'll need to have retroarch already installed) and chimerasnes_libretro.info to /partition:/app/RETROVITA/info.

For better performance, go to retroarch's settings and change these values:
- Video > Threaded Video = ON (default is OFF)
- Audio > Output > Audio Latency (ms) = 128 (default is 64)
- Audio > Resampler > Audio Resampler = sinc (this is the default)
- Audio > Resampler > Resampler Quality = Lowest (default is Lower)
EOF
7z a -r -mx9 chimerasnes.zip chimerasnes
