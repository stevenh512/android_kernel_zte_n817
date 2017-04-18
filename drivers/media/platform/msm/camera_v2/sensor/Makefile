ccflags-y += -Idrivers/media/platform/msm/camera_v2
ccflags-y += -Idrivers/media/platform/msm/camera_v2/msm_vb2
ccflags-y += -Idrivers/media/platform/msm/camera_v2/camera
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/io
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/cci
#obj-$(CONFIG_MSMB_CAMERA) += cci/ io/ csiphy/ csid/ actuator/ flash/ eeprom/
obj-$(CONFIG_MSMB_CAMERA) += cci/ io/ csiphy/ csid/ 
obj-$(CONFIG_MSM_CAMERA_SENSOR) += msm_sensor.o
obj-$(CONFIG_S5K3L1YX) += s5k3l1yx.o
obj-$(CONFIG_IMX135) += imx135.o
obj-$(CONFIG_IMX134) += imx134.o
obj-$(CONFIG_OV8825) += ov8825.o
obj-$(CONFIG_OV8865) += ov8865.o
obj-$(CONFIG_s5k4e1) += s5k4e1.o
obj-$(CONFIG_OV12830) += ov12830.o
obj-$(CONFIG_OV2720) += ov2720.o
obj-$(CONFIG_OV9724) += ov9724.o
obj-$(CONFIG_HI256) += hi256.o
obj-$(CONFIG_OV5648) += ov5648.o
obj-$(CONFIG_MT9M114) += mt9m114.o
obj-$(CONFIG_GC0339) += gc0339.o
#weilanying add ar0542 20130927
obj-$(CONFIG_AR0542) += ar0542.o
obj-$(CONFIG_AT2250) += at2250.o
obj-$(CONFIG_OV8835) += ov8835.o
obj-$(CONFIG_S5K5E2) += s5k5e2.o
#caidezun add sp2529 20140121
obj-$(CONFIG_SP2529) += sp2529.o
#yichangming add ov9760 20131213
obj-$(CONFIG_OV9760) += ov9760.o
#lvcheng add ov9740 20140115
obj-$(CONFIG_OV9740) += ov9740.o
#lvcheng add sp1628 20140227
obj-$(CONFIG_SP1628) += sp1628.o
#
obj-$(CONFIG_GC0310) += gc0310.o
#lvcheng add sp0a20 20141020
obj-$(CONFIG_SP0A20) += sp0a20.o
obj-$(CONFIG_OV2680) += ov2680.o

#
# Modified for camera sensor by ZTE_JIA_20130627 jia.jia
# To avoid side effect on initialization of camera sensor
#
obj-$(CONFIG_MSMB_CAMERA) += eeprom/
obj-$(CONFIG_MSMB_CAMERA) += actuator/
obj-$(CONFIG_MSMB_CAMERA) += flash/
