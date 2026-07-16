
//:IMPORT std/stdObject.js

function stdEvent(type,source,msg,ignore)
{
    if ( arguments.length == 0 )
        return;
    stdObject.apply(this);
    this.type = type;
    if ( source )
	this.source = source;
    if ( msg )
	this.msg = msg;
    if ( ignore )
	this.ignore = ignore;
}

stdObject.defClass
(
    "stdEvent",
    stdEvent,
    stdObject,
    {
	copy : function() {
	    return new stdEvent(this.type,this.source,
			       stdObject.copyHash(this.msg),this.ignore);
	}
    }
);



