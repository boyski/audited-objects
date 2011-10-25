# This file contains makefile syntax which may optionally be used by AO.

# Request oneshell mode. This will only work with GNU make 3.82+
# and possibly some other make variants, and will be silently ignored
# by any other program.
.ONESHELL:

# In the rare case where the main makefile contains a pattern
# rule like "%.mk:", it's necessary to override that with a more
# specific (and useless) rule for this makefile to build itself.
$(lastword $(MAKEFILE_LIST)): ; @:
