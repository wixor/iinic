class AckDeferred:
  PENDING  = 0
  RESOLVED = 1
  FAILED   = 2

  def __init__(self, frame):
    self._frame = frame
    self._status = AckDeferred.PENDING
    self._acknowledgedOn   = None
    self._successCallbacks = []
    self._failCallbacks = []

  def failed(self):
    return self._status == AckDeferred.FAILED

  def successful(self):
    return self._status == AckDeferred.RESOLVED

  def waiting(self):
    return self._status == AckDeferred.PENDING

  def resolve(self, acknowledgedOn):
    self._status = AckDeferred.RESOLVED
    self._acknowledgedOn = acknowledgedOn

    for callback in self._successCallbacks:
      self.runSuccessCallback(callback)

    del(self._successCallbacks)

  def reject(self):
    self._status = AckDeferred.FAILED
    
    for callback in self._failCallbacks:
      self.runFailureCallback(callback)

    del(self._failCallbacks)

  def runSuccessCallback(self, callback):
    callback(self._frame)

  def runFailureCallback(self, callback):
    callback(self._frame, self._acknowledgedOn)

  def promise(self):
    return AckPromise(self, self._successCallbacks, self._failCallbacks)

class AckPromise:
  def __init__(self, deferred, successCallbacks, failCallbacks):
    self._successCallbacks = successCallbacks
    self._failCallbacks = failCallbacks
    self._deferred = deferred

  def onSuccess(self, callback):
    if self._deferred.waiting():
      self._successCallbacks.append(callback)

    if self._deferred.successful():
      self._deferred.runSuccessCallback(callback)

    return self

  def onFailure(self, callback):
    if self._deferred.waiting():
      self._failCallbacks.append(callback)

    if self._deferred.failed()
      self._deferred.runFailureCallback(callback)

    return self
