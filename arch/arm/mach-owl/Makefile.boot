   zreladdr-y	+= $(shell printf "0x%x" $$(( ${CONFIG_KERNEL_LOAD_OFFSET} + 0x00008000 )) )
params_phys-y	:= $(shell printf "0x%x" $$(( ${CONFIG_KERNEL_LOAD_OFFSET} + 0x00000100 )) )
initrd_phys-y	:= $(shell printf "0x%x" $$(( ${CONFIG_KERNEL_LOAD_OFFSET} + 0x00800000 )) )

