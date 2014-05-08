from OurException import OurException

class Proto:
    def __init__(self, frameLayer):
        self.frameLayer = frameLayer
        self.dispatcher = None
    
    # implement this
    def handleFrame(self, frame):
        raise OurException('handling frames not implemented!')
    
    def doRegistration(self, dispatcher):
        self.dispatcher = dispatcher
        self.uponRegistration(dispatcher)
        
    # implement this
    def uponRegistration(self, dispatcher):
        pass