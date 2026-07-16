
//:IMPORT std/stdObject.js
//:IMPORT ts/tinyState.js
//:IMPORT std/stdEvent.js


function ifRCTwebView(
    parent,RCTparent)
{
    if ( arguments.length == 0 )
        return;
    this.i_RCTparent = RCTparent;
    tinyState.apply(this,[parent]);
    this.initial("ifRCTwebView");
}

stdObject.defClass
(
    "ifRCTwebView",
    ifRCTwebView,
    tinyState,
    {
	send : function(message) {
	    var msg = JSON.parse(message);
	    var body = "";
	    if ( msg.body )
		body = msg.body;
	    var tmsg = msg.method+" "+msg.uri+" HTTP/1.1\n\n"+body;
	    this.parent.send(this,msg.seq,tmsg);
	},
	recv : function(seq,status,message) {
	    message = message.replace(/`/g,"`+'`'+`");
	    var sts = 'OK';
	    if ( status != 200 )
		sts = 'ERROR';
	    var runScript = `
		{
		    window.gtApp.refShared('ifReactNative')
			.que[`+seq+`].recv({
			status : '`+sts+`',
			status2 : `+status+`,
			message : `+'`'+message+'`'+`
			});
		}
`;
console.log("inect = --- "+runScript+" ---\n");
	    this.i_RCTparent.webref.injectJavaScript(runScript);
	},
    }
);


