def main():
    comm = iinic.USBComm() if Config.ON_DEVICE else iinic.NetComm()
    nic = iinic.NIC(comm)
    frameLayer = FrameLayer(nic)
    myId = frameLayer.getMyId()
    print >> sys.stderr, 'NIC is working, ID = ', frameLayer.getMyId()
    dispatcher = Dispatcher(frameLayer)

