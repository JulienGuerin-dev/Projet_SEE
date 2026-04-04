################################################################################
# Buildroot package for local ultrason kernel module
################################################################################

ULTRASON_VERSION = 1.0
ULTRASON_SITE = package/ultrason
ULTRASON_SITE_METHOD = local

ULTRASON_MODULE_MAKE_OPTS = \
	KERNELDIR=$(LINUX_DIR)


$(eval $(kernel-module))
$(eval $(generic-package))
