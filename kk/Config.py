ID_LENGTH = 2
class API_VERSION:
    OLD = 1.0
    NEW = 2.0
    
    
# modify it    
ON_DEVICE = False
CURRENT_VERSION = API_VERSION.NEW if ON_DEVICE else API_VERSION.OLD