ifneq (,$(findstring install, $(MAKECMDGOALS)))
    BUILD_METHOD := RELEASE
else ifneq (,$(findstring release, $(MAKECMDGOALS)))
    BUILD_METHOD := RELEASE
else
    BUILD_METHOD := DEBUG
endif
