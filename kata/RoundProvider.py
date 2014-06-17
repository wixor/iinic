from ..kk.Frame import Frame, FrameLayer
import Config

class RoundProvider:
    def __init__(self):
        pass

    def getRoundNumber(self):
        if self.dispatcher.timeManager.synced():
            return int(self.dispatcher.timeManager._localToNetwork(self.dispatcher.timeManager.getApproxNow()) / self.getRoundDuration())
        return -1

    def getRoundDuration(self):
        if 'roundDuration' not in self.__dict__:
            self.roundDuration = (255.0 + Frame.lengthOverhead() + Config.DEVICE_BYTES_SILENCE_BEFORE + Config.DEVICE_BYTES_SILENCE_AFTER) * self.dispatcher.frameLayer.get_byte_send_time() * 1000000
            pow10 = 1
            while self.roundDuration > pow10*10:
                pow10 *= 10
            self.roundDuration = int(self.roundDuration / pow10 + 1) * pow10
        return self.roundDuration

    def scheduleFrame(self, frame, roundNumber, forceOffset=0):
        drounds = roundNumber - self.getRoundNumber()
        if drounds < 1: return
        self.dispatcher.frameLayer.sendWholeFrame(frame, (self.getRoundNumber() + drounds) * self.getRoundDuration())
    
