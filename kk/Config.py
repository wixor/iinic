ID_LENGTH = 2
class API_VERSION:
    OLD = 1.0
    NEW = 2.0

# old api artifacts
TIMING_VARIANCE = 10
INNER_DEADLINE = 1.0
    
# modify it    
ON_DEVICE = False
CURRENT_VERSION = API_VERSION.NEW if ON_DEVICE else API_VERSION.OLD
