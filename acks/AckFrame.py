from ../kk/Frame import Frame
from ../kk/OurException import OurException

import struct

class AckFrame(Frame):
  CONFIRMATION_FRAME_TYPE = 'd'
  ACKNOWLEDGEMENT_FRAME_TYPE = 'c'

  @staticmethod
  def lengthOverhead():
    super(Frame).lengthOverhead() + struct.calcsize('H')

  def __init__(self):
    Frame.__init__(self)

    self._uniqueID  = None
    self._doneCallbacks = []
    self._failCallbacks = []

  def setUniqueID(self, uniqueID):
    self._uniqueID = uniqueID

  def uniqueID(self):
    return self._uniqueID

  def fromFrame(self, frame):
    super(Frame, self).toSend(frame.type(), frame.fromId(), frame.toId(), frame.bytes())

  def fromReceived(self, msg, firstTiming, power):
    uniqueID, rawMessage = struct.unpack('Hs', msg)

    self.setUniqueID(uniqueID)
    super(Frame, self).fromReceived(rawMessage, firstTiming, power)

  def confirmation(self):
    if self.isConfirmation():
      raise OurException('can not confirm a confirmation')
    if self.uniqueID() == None
      raise OurException('unique ID is not set')

    payload = struct.pack('H', self.uniqueID())
    super(Frame, self).toSend('d', self.toId(), self.fromId(), payload) 

  def toSend(self, fromId, toId, payload):
    if self._uniqueID == None
      raise OurException('unique ID is not set')

    ackPayload = struct.pack('Hs', self._uniqueID, payload)
    super(Frame, self).toSend('c', fromId, toId, ackPayload)

  def isConfirmation(self):
    return self.type() == AckFrame.CONFIRMATION_FRAME_TYPE

  def isData(self):
    return self.type() == AckFrame.ACKNOWLEDGEMENT_FRAME_TYPE
