/*
 * sharedImage.js
 *
 * EventDance examples
 *
 * Authors:
 *   Eduardo Lima Mitev <elima@igalia.com>
 */

imports.searchPath.unshift (".");
imports.searchPath.unshift ("../common");

const MainLoop = imports.mainloop;
const Lang = imports.lang;
const Evd = imports.gi.Evd;
const Json = imports.json.Json;
const SharedImageServer = imports.sharedImageServer.SharedImageServer;

const PORT = 8080;

Evd.tls_init ();

/* shared image server */
let sis = new SharedImageServer ({rotate: true});

sis.setPeerOnUpdate (
    function (peer, updateObj) {
        let cmd = ["update", updateObj];
        let msg = Json.encode (cmd);
        peer.transport.send_text (peer, msg);
    }, null);


/* peer manager */
let peerManager = Evd.PeerManager.get_default ();

function peerOnClose (peerManager, peer) {
    let index = sis.releaseViewport (peer);
    print ("Release viewport " +  index + " by peer " + peer.id);
}

function peerOnOpen (peerManager, peer) {
    let index = sis.acquireViewport (peer);
    print ("New peer " + peer.id + " acquired viewport " + index);

/*
    let msg = '["set-index", '+index+']';
    peer.transport.send_text (peer, msg);
*/
}

peerManager.connect ("new-peer", peerOnOpen);
peerManager.connect ("peer-closed", peerOnClose);

/* web transport */
let transport = new Evd.WebTransport ();

function peerOnReceive (t, peer) {
    let data = peer.transport.receive_text (peer);

    let cmd = Json.decode (data, true);
    if (cmd === null)
        return;

    switch (cmd[0]) {
    case "req-update":
        sis.updatePeer (peer, sis.UPDATE_ALL);
        break;

    case "grab":
        sis.grabImage (peer, cmd[1]);
        break;

    case "ungrab":
        sis.ungrabImage (peer);
        break;

    case "move":
        sis.moveImage (peer, cmd[1]);
        break;
    }
}

transport.connect ("receive", peerOnReceive);

/* web dir */
let webDir = new Evd.WebDir ({ root: "../common" });

/* web selector */
let selector = new Evd.WebSelector ();

//selector.tls_autostart = true;
//selector.tls_credentials.dh_bits = 1024;
selector.tls_credentials.cert_file = "../../tests/certs/x509-server.pem";
selector.tls_credentials.key_file = "../../tests/certs/x509-server-key.pem";

selector.set_default_service (webDir);

transport.selector = selector;

selector.listen_async ("0.0.0.0:" + PORT, null,
    function (service, result) {
        if (service.listen_finish (result))
            print ("Listening, now point your browser to http://localhost:" + PORT + "/shared_image.html");
    }, null);

/* start the show */
MainLoop.run ("main");

Evd.tls_deinit ();