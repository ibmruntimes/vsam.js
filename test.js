const vsam = require("./build/Release/vsam.node");
const async = require('async');


function printUntilEnd(file) {
  var end = false;
  async.whilst(
    // Stop at end of file
    () => { return !end },

    // Read the next record
    (callback) => {
      file.read( (record, err) => {
        if (record == null)
          end = true;
        else {
          var obj = new Object;
          obj.key= record.key.toString();
          obj.name = record.data.toString('utf8', 0, 10);
          obj.gender = record.data.toString('utf8', 10, 20);
          console.log(JSON.stringify(obj));
        }
        callback(null);
      });
    },

    // Finally close
    (err) => {
      if (err)
        console.log("Error:" + err);
      file.close((err) => {});
    }
  );
}

vsam("//'BARBOZA.TEST.VSAM.KSDS'", (file) => {

  file.find( "00100", (record, err) => {
    var obj = new Object;
    obj.key = record.key.toString();
    obj.name = record.data.toString('ascii', 0, 10);
    obj.gender = record.data.toString('ascii', 10, 20);
    console.log(JSON.stringify(obj));
    printUntilEnd(file);
  });

});

