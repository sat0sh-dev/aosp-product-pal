# PAL (Protocol Adaptation Layer) Module
#
# This makefile adds the PAL daemon to the product image

PRODUCT_PACKAGES += \
    pal_daemon \
    pal_vcs_mac_test \
    pal2vcs_daemon \
    pal2vcs_vsomeip_config

# SELinux policy
PRODUCT_PRIVATE_SEPOLICY_DIRS += \
    product/pal/sepolicy
