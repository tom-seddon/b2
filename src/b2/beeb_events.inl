//#define ENAME BeebEventType
//EBEGIN()
//#define BEEB_EVENT_TYPE EPN
//#include "beeb_events_types.inl"
//EEND()
//#undef ENAME
//
//#define ENAME BeebEventTypeFlag
//EBEGIN()
//// ShownInUI events are shown... in the UI.
////
//// ShownInUI events are implicitly ChangesTimeline events.
//EPNV(ShownInUI,1<<0)
//
//// ChangesTimeline events cause Timeline::Changed to be called when
//// they're added or replayed.
//EPNV(ChangesTimeline,1<<1)
//
//// Synthetic events are inserted into the replay data to simplify the
//// code. They never appear on the timeline.
//EPNV(Synthetic,1<<2)
//
//// Start events are suitable for use as the first event in a replay.
//// (This flag is used in asserts.)
//EPNV(Start,1<<3)
//
//// A deletable event can be removed from the timeline without causing
//// replay problems.
//EPNV(CanDelete,1<<4)
//
//EEND()
//#undef ENAME
