# PAL (Protocol Adaptation Layer) Module
#
# This makefile adds the PAL daemon to the vendor image

PRODUCT_PACKAGES += \
    pal_daemon

# SELinux policy
PRODUCT_PRIVATE_SEPOLICY_DIRS += \
    vendor/pal/sepolicy
