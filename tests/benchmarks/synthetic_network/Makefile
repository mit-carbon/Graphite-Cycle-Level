TARGET=synthetic_network
SOURCES = synthetic_network.cc

MODE ?=
ENABLE_SM ?= false

APP_SPECIFIC_CXX_FLAGS ?= -I$(SIM_ROOT)/common/core \
								  -I$(SIM_ROOT)/common/core/memory_subsystem \
								  -I$(SIM_ROOT)/common/network \
								  -I$(SIM_ROOT)/common/network/components \
								  -I$(SIM_ROOT)/common/network/components/router \
								  -I$(SIM_ROOT)/common/network/components/router/performance \
								  -I$(SIM_ROOT)/common/network/components/router/power \
								  -I$(SIM_ROOT)/common/network/components/link \
								  -I$(SIM_ROOT)/common/network/components/link/performance \
								  -I$(SIM_ROOT)/common/network/components/link/power \
								  -I$(SIM_ROOT)/common/network/components/router/performance/network_msg_types \
								  -I$(SIM_ROOT)/common/network/components/router/performance/network_msg_types/flit_types \
								  -I$(SIM_ROOT)/common/network/components/router/performance/network_msg_types/buffer_management_msg_types \
								  -I$(SIM_ROOT)/common/network/components/router/performance/flow_control_schemes \
								  -I$(SIM_ROOT)/common/network/components/router/performance/buffer_models \
								  -I$(SIM_ROOT)/common/network/components/router/performance/buffer_status \
								  -I$(SIM_ROOT)/common/network/models \
								  -I$(SIM_ROOT)/common/transport \
								  -I$(SIM_ROOT)/common/performance_model \
								  -I$(SIM_ROOT)/common/performance_model/queue_models \
								  -I$(SIM_ROOT)/common/performance_model/memory_subsystem \
								  -I$(SIM_ROOT)/common/system \
								  -I$(SIM_ROOT)/os-services-25032-gcc.4.0.0-linux-ia32_intel64 \
								  -I$(SIM_ROOT)/common/config

include ../../Makefile.tests
