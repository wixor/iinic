from OurException import OurException

class Proto:
    # assign this
    frameTypes = None
    
    def __init__(self):
        self.frameLayer = None
        self.dispatcher = None
    
    # implement this
    def handleFrame(self, frame):
        raise OurException('handling frames not implemented!')
    
    def doRegistration(self, dispatcher):
        self.dispatcher = dispatcher
        self.frameLayer = dispatcher.frameLayer
        self.timeManager = dispatcher.timeManager
        
    # implement this
    def onStart(self):
        pass