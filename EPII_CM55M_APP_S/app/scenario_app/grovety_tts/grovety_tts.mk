override SCENARIO_APP_SUPPORT_LIST := $(APP_TYPE)


APPL_DEFINES += -DGROVETY_TTS
#APPL_DEFINES += -DIP_xdma
APPL_DEFINES += -DEVT_DATAPATH
APPL_DEFINES += -DETHOSU_ARCH=u55

#APPL_DEFINES += -DEVT_CM55MTIMER -DEVT_CM55MMB
APPL_DEFINES += -DDBG_MORE


##
# library support feature
# Add new library here
# The source code should be loacted in ~\library\{lib_name}\
##
LIB_SEL = pwrmgmt tflmtag2412_u55tag2411 spi_eeprom

##
# middleware support feature
# Add new middleware here
# The source code should be loacted in ~\middleware\{mid_name}\
##

override undefine OS_SEL
override TRUSTZONE := y
override TRUSTZONE_TYPE := security
override TRUSTZONE_FW_TYPE := 1
override CIS_SEL := HM_COMMON
override EPII_USECASE_SEL := drv_user_defined

override LINKER_SCRIPT_FILE := $(SCENARIO_APP_ROOT)/$(APP_TYPE)/grovety_tts.ld


override MODEL_VARIANT := stories_7m_fc_ethos-u55-64

APPL_DEFINES += \
	-DMODEL_ENABLE_NUMERIC_TEST

APPL_APP_INCDIRS := \
	$(SCENARIO_APP_ROOT)/grovety_tts/stories/$(MODEL_VARIANT) \
	$(SCENARIO_APP_ROOT)/grovety_tts/stories/src

APPL_APP_CXXSRCS := \
	$(call get_cxxsrcs, $(SCENARIO_APP_ROOT)/grovety_tts/stories/src) \
	$(call get_cxxsrcs, $(SCENARIO_APP_ROOT)/grovety_tts/stories/$(MODEL_VARIANT))


#APPL_DEFINES += \
#	-DPROFILE_TFLITE_GRAPH

APPL_DEFINES += \
	-DEXCHANGE_OVER_I2C

APPL_DEFINES += \
	-DENABLE_TTS


APPL_DEFINES += \
	-Wno-write-strings \
	-Werror=return-type \
	-u _printf_float

USE_APP_MAIN_MK = y

##
# platform (onchip ip) support feature
# Add all of supported ip list here
# The source code should be located in ~\drivers\{ip_name}\
##

DRIVERS_IP_LIST		?= 2x2 \
					5x5 \
					uart spi \
					i3c_mst isp \
					iic \
					mb \
					scu \
					timer \
					watchdog \
					rtc	\
					cdm \
					edm \
					jpeg \
					xdma \
					dp \
					inp \
					tpg \
					inp1bitparser \
					sensorctrl \
					gpio \
					i2s \
					pdm \
					i3c_slv \
					vad \
					swreg_aon \
					swreg_lsc \
					dma \
					ppc \
					pmu \
					mpc  \
					hxautoi2c_mst \
					sensorctrl \
					csirx \
					csitx \
					adcc \
					pwm \
					inpovparser \
					adcc_hv  \
					u55

DRIVERS_IP_INSTANCE  ?= RTC0 \
						RTC1 \
						RTC2 \
						TIMER0 \
						TIMER1 \
						TIMER2 \
						TIMER3 \
						TIMER4 \
						TIMER5 \
						WDT0 \
						WDT1 \
						DMA0 \
						DMA1 \
						DMA2 \
						DMA3 \
						UART0 \
						UART1 \
						UART2 \
						IIC_HOST_SENSOR \
						IIC_HOST \
						IIC_HOST_MIPI \
						IIC_SLAVE0 \
						IIC_SLAVE1 \
						SSPI_HOST \
						QSPI_HOST \
						OSPI_HOST \
						SSPI_SLAVE \
						GPIO_G0 \
						GPIO_G1 \
						GPIO_G2 \
						GPIO_G3 \
						SB_GPIO \
						AON_GPIO \
						I2S_HOST \
						I2S_SLAVE \
						IIIC_SLAVE0 \
						IIIC_SLAVE1 \
						PWM0 \
						PWM1 \
						PWM2 \
						ADCC \
						ADCC_HV

ifneq ($(IC_VER), 10)
DRIVERS_IP_INSTANCE  += TIMER6 \
						TIMER7 \
						TIMER8
endif

DRIVERS_IP_NS_INSTANCE ?=
