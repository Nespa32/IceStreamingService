module StreamingService
{
    sequence<string> StringList;
    
    struct StreamEntry
    {
        string streamName;
        string endpoint;
        string videoSize;
        int bitRate;
        StringList keyword;
    };

    sequence<StreamEntry> StreamList;
    
    interface PortalInterface
    {
        // For streamers
        void NewStream(StreamEntry entry);
        void CloseStream(StreamEntry entry);
        // For clients
        StreamList GetStreamList();
    };

    interface StreamNotifierInterface
    {
        void NotifyStreamAdded(StreamEntry entry);
        void NotifyStreamRemoved(StreamEntry entry);
    };
};
