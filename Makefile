PACKAGE = test-capture
VERSION = 0.0.5

abs_top_srcdir = $(dir $(abspath $(firstword $(MAKEFILE_LIST))))
VPATH += $(abs_top_srcdir)

PKGCONFIG = pkg-config
PKG_LIBPNG = libpng
INSTALL = install
INSTALL_PROG = ${INSTALL} -p -m 0755
MKDIR_P = ${INSTALL} -d -m 0755
XZ = xz
TAR = tar
TAR_FLAGS = --owner root --group root --mode a+rX,go-w

PKCONFIG_FLAGS = $(patsubst -%,--%,$(filter -static,${LDFLAGS}))

PNG_CPPFLAGS = $(shell ${PKGCONFIG} --cflags ${PKG_LIBPNG})
PNG_LIBS = $(shell ${PKGCONFIG} ${PKCONFIG_FLAGS} --libs ${PKG_LIBPNG})
JPEG_CPPFLAGS =
JPEG_LIBS = -ljpeg

IMG_ROWS = 256
IMG_COLS = 256
PERF_CNT = 1000

AM_CPPFLAGS = \
	${PNG_CPPFLAGS} \
	${JPEG_CPPFLAGS} \

AM_CFLAGS = -std=gnu99
CFLAGS = -g3 -O3 -Wall -W -Werror -Wno-unused-parameter

LDLIBS = \
	${PNG_LIBS} \
	${JPEG_LIBS} \

_yuv422_SOURCES = \
	yuv422rgb888.h \
	yuv422rgb888.c \
	yuv422rgb888_generic.inc.c \
	yuv422rgb888_neon.inc.c \

SOURCES = \
	capture.c \
	capture.h \
	filter-jpg.c \
	filter-jpg_samp.inc.h \
	filter-png.c \
	util.c \
	${_yuv422_SOURCES}

TESTSUITE_RUN_FILTER_SOURCES = \
	testsuite/run-filter.c \
	util.c \
	filter-jpg.c \
	filter-jpg_samp.inc.h \
	filter-png.c \
	${_yuv422_SOURCES}

TESTSUITE_YUV2RGB_PERF_SOURCES = \
	testsuite/yuv2rgb-perf.c \
	util.c \
	${_yuv422_SOURCES}

TEST_SOURCES = \
	testsuite/gen-img \
	${TESTSUITE_RUN_FILTER_SOURCES} \
	$(TESTSUITE_YUV2RGB_PERF_SOURCES)

TEST_PROGRAMS = \
	testsuite/run-filter \
	testsuite/yuv2rgb-perf

TEST_IMAGES = blue green red white
_test_gen_images = \
	$(addprefix testsuite/, \
		$(addsuffix .ycbcr,$(TEST_IMAGES)) \
		$(addsuffix .yuv,$(TEST_IMAGES)))

_test_inp_images = \
	${_test_gen_images}

_test_out_images = \
	$(addsuffix .png,${_test_inp_images}) \
	$(addsuffix .jpg,${_test_inp_images})

TEST_GEN_DATA = \
	$(_test_out_images)

build_c = \
	$(CC) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) $(AM_LDFLAGS) $(LDFLAGS) $(filter %.c,$(filter-out %.inc.c,$^)) -o $@ $(LDLIBS)

all:	capture tests

capture:	${SOURCES}
	$(call build_c)

testsuite/run-filter:	${TESTSUITE_RUN_FILTER_SOURCES} Makefile | testsuite/.dirstamp
	$(call build_c)

testsuite/yuv2rgb-perf:	${TESTSUITE_YUV2RGB_PERF_SOURCES} Makefile | testsuite/.dirstamp
	$(call build_c)

tests:	${TEST_PROGRAMS} ${_test_out_images} .perf-data

testsuite/.dirstamp:
	mkdir -p ${@D}
	@touch $@

$(_test_gen_images):		testsuite/.gen-images.stamp
testsuite/.gen-images.stamp:	testsuite/gen-img Makefile | testsuite/.dirstamp
	bash $< ${IMG_COLS} ${IMG_ROWS} ${@D}
	@touch $@

.perf-data:	testsuite/yuv2rgb-perf testsuite/red.yuv testsuite/green.yuv testsuite/blue.yuv testsuite/white.yuv
	cat $(filter %.yuv,$^) | ${QEMU} $(abspath $<) ${IMG_COLS} $$(( ${IMG_ROWS} * 4 )) 8 ${PERF_CNT} 3>/dev/null

_gen_img = \
	$(QEMU) $(abspath $<) $1 ${IMG_COLS} ${IMG_ROWS} 8 $$(( ${IMG_COLS}*2 )) <$* 3>$@.tmp

$(filter %.png,${_test_out_images}):%.png:	testsuite/run-filter %
	@rm -f $@.tmp
	$(call _gen_img,png)
	@mv $@.tmp $@

$(filter %.jpg,${_test_out_images}):%.jpg:	testsuite/run-filter %
	@rm -f $@.tmp
	$(call _gen_img,jpg)
	@mv $@.tmp $@

clean:
	rm -f ${TEST_GEN_DATA} ${_test_inp_images} testsuite/.*stamp
	rm -f ${TEST_PROGRAMS} capture
	-rmdir testsuite

install:	capture
	${MKDIR_P} ${DESTDIR}${bindir}
	${INSTALL_PROG} -p capture ${DESTDIR}${bindir}/test-capture

dist:	${PACKAGE}-${VERSION}.tar.xz

%.tar.xz:	%.tar
	@rm -f $@
	${XZ} -c $< > $@

${PACKAGE}-${VERSION}.tar:	Makefile ${SOURCES} ${TEST_SOURCES}
	${TAR} cf $@ ${TAR_FLAGS} --transform 's!^!${PACKAGE}-${VERSION}/!' $(sort $^)
