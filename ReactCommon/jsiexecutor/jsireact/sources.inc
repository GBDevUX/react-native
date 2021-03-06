LIBLETNAME = jsireact

!include $(OPENSOURCE_REACTNATIVE)\OfficeISS\ReactCommon\make.inc

INCLUDES = $(INCLUDES); \
	$(OPENSOURCE_REACTNATIVE)\OfficeISS\ReactCommon\jsiexecutor; \

SOURCES_SHARED = \
	$(SOURCES_SHARED) \
	..\JSIExecutor.cpp \
	..\JSINativeModules.cpp \

SOURCES_ANDROID = $(SOURCES_SHARED) \

SOURCES_WIN32 = $(SOURCES_SHARED)

SOURCES_WINRT = $(SOURCES_SHARED)
