
//:IMPORT std/stdObject.js
//:IMPORT ts/tinyState.js
//:IMPORT std/stdEvent.js
import { NativeModules } from 'react-native';

/*
This is call one time from top of react native javascripts
*/


function gtRCTapplication(
    dummy)
{
    if ( arguments.length == 0 )
        return;
    this.i_que = [];
    this.i_bridge = NativeModules.gtFrameworkBridgeRCT;
    tinyState.apply(this,[null,1]);
    this.initial("gtRCTapplication");
}

stdObject.defClass
(
    "gtRCTapplication",
    gtRCTapplication,
    tinyState,
    {
	i_initial : null,
	i_que : null,
	i_active : false,


	send : function(wv,seq,message) {
	    if ( !this.i_active ) {
		wv.recv(seq,
			501,
			"system is not ready");
		return;
	    }
	    var i;
	    for ( i = 0 ; i < this.i_que.length ; i ++ )
		if ( !this.i_que[i] )
		    break;
	    var info = [];
	    info.webView = wv;
	    info.seq = seq;
	    this.i_que[i] = info;
	    if ( !message )
		message = " ";
	    this.i_bridge.send(i,message,
			       (err,events) => {
				   var info = this.i_que[events.seq];
				   if ( events.status == 0 ) {
				       var res = events.message.match(/[ \t][0-9]+[ \t]/);
				       res = res[0].match(/[0-9]+/);
				       var p = events.message.indexOf("\n\n",0);
				       events.message
					   = events.message.substr(p+2);
				       info.webView.recv(
					   info.seq,
					   Number(res[0]),
					   events.message);
				   }
				   else {
				       info.webView.recv(
					   info.seq,
					   500,
					   events.message);
				   }
				   this.i_que[events.seq] = null;
				   this.wakeup();
			       });
	},

	/*=====================================================
	 *
	 *    tinyState STATE MACHINE
	 *
	 *=====================================================*/


	INI_START : function(ev) {
	    this.i_bridge.create(0,
				 (err,events) => {
				     this.i_initial = events;
				     this.wakeup();
				 });
	    return "doINI_WAIT";
	},
	INI_WAIT : function(ev) {
	    if ( !this.i_initial )
		return;
	    this.i_active = true;
	    return "doACT_START";
	},
	FIN_START : function(ev) {
	    if ( this.i_que.length )
		return;
	    this.i_active = false;
	    return "doFIN_TINYSTATE_START";
	}
    }
);


