# PAL (Protocol Adaptation Layer) Module
#
# This makefile adds the PAL daemon to the vendor image

PRODUCT_PACKAGES += \
    pal_daemon \
    pal_vcs_mac_test

# SELinux policy
PRODUCT_PRIVATE_SEPOLICY_DIRS += \
    vendor/pal/sepolicy
