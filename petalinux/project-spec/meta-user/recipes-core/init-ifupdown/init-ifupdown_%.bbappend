SRC_URI += " \
	file://myinterfaces \
	"
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

#overwrite interface file with the custom xilinxinterfaces file in rootfs
do_install:append(){
	install -m 0644 ${WORKDIR}/myinterfaces ${D}${sysconfdir}/network/interfaces
}
