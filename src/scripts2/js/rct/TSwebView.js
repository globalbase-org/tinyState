


import React, { Component } from 'react';
import { View } from 'react-native';
import { WebView } from 'react-native-webview';
import { ifRCTwebView } from '../if/ifRCTwebView.js';

export default class TSwebView extends Component {
    static ifRCTobj;
    static i_props;
    static webref;

    constructor(props) {
	super(props);
	this.ifRCTobj = new ifRCTwebView(props.bridge,this);
	this.i_props = props;
    }

    render() {
	return (
        <WebView
            ref={(r) => (this.webref = r)}
            source={this.i_props.source}
	    onMessage={(event) => {this.ifRCTobj.send(event.nativeEvent.data);}}
		/>
	);
  }
}
