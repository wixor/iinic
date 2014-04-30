class FrameBuilder:
  def __init__(self, concreteFrameFormat):
    self.frameFormat = concreteFrameFormat

    self.nextFrameAttributes = {}
    self.nextFrameAttributes["formatSpecific"] = {}

  def from(self, senderAddr):
    self.nextFrameAttributes["sender"] = senderAddr
    return self 

  def to(self, receiverAddr):
    self.nextFrameAttributes["receiver"] = receiverAddr
    return self

  def with(self, key, value):
    self.nextFrameAttributes["formatSpecific"][key] = value
    return self 

  def create(self, data):

