from ../kk/Frame import Frame
from ../kk/OurException import OurException

import struct

class AckFrame(Frame):
  CONFIRMATION_FRAME_TYPE = 'd'
  ACKNOWLEDGEMENT_FRAME_TYPE = 'c'

  def __init__(self, uniqueID = None):
    self._doneCallbacks = []
    self._failCallbacks = []
    self.uniqueID = uniqueID
    
  def toSend(self, uniqueID, kwargs):
    self.uniqueID = uniqueID
    kwargs['payload'] = struct.pack('Hs', (uniqueID, kwargs['payload']))
    for f in kwargs:
        self[f] = kwargs[f]
    return self

  def fromFrame(self, frame):
    for f in frame:
        self[f] = frame[f]
    self.uniqueID = struct.unpack('H', self['payload'][0:4])
    return self

  def getData(self):
      return self['payload'][4:]

  def confirmation(self):
    if self.isConfirmation():
      raise OurException('cannot confirm a confirmation')
    if self.uniqueID is None:
      raise OurException('unique ID is not set')

    return AckFrame().toSend(self.uniqueID, {
        'ftype': AckFrame.CONFIRMATION_FRAME_TYPE,
        'fromId': self['toId'],
        'toId': self['fromId'],
        'payload': ''}) # no payload, right?

  def isConfirmation(self):
    return self.ftype() == AckFrame.CONFIRMATION_FRAME_TYPE

  def isData(self):
    return self.ftype() == AckFrame.ACKNOWLEDGEMENT_FRAME_TYPE
