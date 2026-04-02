# could potentially figure out the branches using git branch -a
# --format='%(refname:short)'... but the list is not all that long.

.PHONY:_ffmpeg_releases
_ffmpeg_releases: export FFMPEG_UPSTREAM=git@github.com:FFmpeg/FFmpeg
_ffmpeg_releases: export FFMPEG_DEST:=/tmp/ffmpeg
_ffmpeg_releases: export FFMPEG_MIRROR:=$(FFMPEG_DEST)/FFmpeg.mirror
_ffmpeg_releases:
	$(MAKE) _ffmpeg_mirror
	$(MAKE) _ffmpeg_release VERSION=4.0
	$(MAKE) _ffmpeg_release VERSION=4.1
	$(MAKE) _ffmpeg_release VERSION=4.2
	$(MAKE) _ffmpeg_release VERSION=4.3
	$(MAKE) _ffmpeg_release VERSION=4.4
	$(MAKE) _ffmpeg_release VERSION=5.0
	$(MAKE) _ffmpeg_release VERSION=5.1
	$(MAKE) _ffmpeg_release VERSION=6.0
	$(MAKE) _ffmpeg_release VERSION=6.1
	$(MAKE) _ffmpeg_release VERSION=7.0
	$(MAKE) _ffmpeg_release VERSION=7.1

.PHONY:_ffmpeg_mirror
_ffmpeg_mirror:
	rm -Rf "$(FFMPEG_DEST)"
	mkdir -p "$(FFMPEG_DEST)"
	cd "$(FFMPEG_DEST)" && git clone --bare "$(FFMPEG_UPSTREAM)" "$(FFMPEG_MIRROR)"

.PHONY:_ffmpeg_release
_ffmpeg_release: VERSION=$(must specify VERSION)
_ffmpeg_release: _DEST=$(FFMPEG_DEST)/FFmpeg.$(VERSION)
_ffmpeg_release:
	rm -Rf "$(_DEST)"
	git clone "$(FFMPEG_MIRROR)" "$(_DEST)"
	cd "$(_DEST)" && git checkout "release/$(VERSION)"
