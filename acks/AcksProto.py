from ../kk/Proto import Proto
from AckFrame import AckFrame
from AckDeferred import AckDeferred
from time import time

class AcksUniqueIDProvider:
  MAX_USINT = 65535

  def __init__(self):
    self.uniqueIDSequences = {}

  def UIDFor(self, toId):
    if self.uniqueIDSequences.get(toId) == None:
      self.uniqueIDSequences[toId] = 0

    uID = self.uniqueIDSequences[toId]
    self.uniqueIDSequences[toId] += 1
    self.uniqueIDSequences[toId] %= AcksUniqueIDProvider.MAX_USINT

    return uID
    
class AcksProto(Proto):
  FRAME_TYPE_TO_ACK = 'c'
  FRAME_TYPE_ACK    = 'd'

  FRAME_TIMEOUT = 1

  def __init__(self, UIDProvider):
    Proto.__init__(self)
    self._deferreds = {}
    self._UIDProvider = UIDProvider

  def onStart(self):
      self._fromId = self.frameLayer.getMyId()
    
  def handleFrame(self, frame):
    ackFrame = AckFrame()
    ackFrame.fromFrame(frame)

    if ackFrame.isData():
      conf = ackFrame.confirmation()
      self.frameLayer.sendWholeFrame(conf)

    elif self.isConfirmation():
      uniqueID = ackFrame.uniqueID
      toId = ackFrame.fromId()

      self._deferreds[toId][uniqueID].resolve(time.time())
      self._deferreds[toId][uniqueID] = None

    # TODO: Make possible to fail ack frame
    # TODO: Handle data :)
      
  def sendFrame(self, toId, message):
    ackFrame = AckFrame()
    ackFrame.toSend(self._UIDProvider.UIDFor(toId), {
        'ftype': FRAME_TYPE_TO_ACK,
        'fromId': self._fromId,
        'toId': toId,
        'payload': message})
    
    self.frameLayer.sendWholeFrame(ackFrame)

    if self._deferreds.get(toId) == None:
      self._deferreds[toId] = {}

    self._deferreds[toId][ackFrame.uniqueID] = AckDeferred(ackFrame)
    return self._deferreds[toId][ackFrame.uniqueID].promise()
