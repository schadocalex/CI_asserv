var express = require('express');
var app = express();
var path = require('path');

app.all('*', function(req, res, next){
	console.log('Connection from '+req.ip+' on : "'+req.path+'"');
	res.set("Access-Control-Allow-Origin", "*");
	next();
});

app.use(express.static('./'));

app.all('*', function(req, res){ //index
	//res.redirect('index.html');
});

var server = app.listen(4000, function() {
	console.log('Listening on port %d', server.address().port);
	console.log(__dirname);
});
server.on("error", function(err) {
	console.log(err);
});
