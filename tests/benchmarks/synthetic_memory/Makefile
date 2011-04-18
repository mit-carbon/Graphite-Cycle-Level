TARGET=synthetic_memory
SOURCES = synthetic_memory.cc

MODE ?=
CORES ?= 64
CONFIG_FILE ?= $(SIM_ROOT)/carbon_sim.cfg

SIM_FLAGS ?= "-c $(CONFIG_FILE) --general/num_processes=1 --general/total_cores=$(CORES) --general/enable_shared_mem=true --general/enable_performance_modeling=false"

APP_SPECIFIC_CXX_FLAGS ?= -I$(SIM_ROOT)/common/core -I$(SIM_ROOT)/common/system -I$(SIM_ROOT)/common/core/memory_subsystem -I$(SIM_ROOT)/common/network -I$(SIM_ROOT)/common/config -I$(SIM_ROOT)/common/performance_model -I$(SIM_ROOT)/common/performance_model/memory_subsystem

include ../../Makefile.tests
