const vsam = require("../build/Release/vsam.node");
const async = require('async');
const fs = require('fs');
const expect = require('chai').expect;
const should = require('chai').should;
const assert = require('chai').assert;

function readUntilEnd(file, done) {
  var end = false;
  async.whilst(
    // Stop at end of file
    () => { return !end },

    // Read the next record
    (callback) => {
      file.read( (record, err) => {
        if (record == null)
          end = true;
        callback(err);
      });
    },

    // Finally close
    (err) => {
      assert.ifError(err);
      expect(file.close()).to.not.throw;
      done();
    }
  );
}

describe("Key Sequenced Dataset", function() {
  it("open and close a vsam file", function(done) {
    vsam( "//'BARBOZA.TEST.VSAM.KSDS'", 
          JSON.parse(fs.readFileSync('test/test.json')),
          (file) => {
            expect(file).to.not.be.null;
            expect(file.close()).to.not.throw;
            done();
          }
    );
  });

  it("reads one record and verifies it's properties", function(done) {
    vsam( "//'BARBOZA.TEST.VSAM.KSDS'", 
          JSON.parse(fs.readFileSync('test/test.json')),
          (file) => {
            file.read( (record, err) => {
              assert.ifError(err);
              expect(record).to.have.property('key');
              expect(record).to.have.property('name');
              expect(record).to.have.property('gender');
              expect(file.close()).to.not.throw;
              done();
            });
          });
  });

  it("reads all records until the end", function(done) {
    vsam( "//'BARBOZA.TEST.VSAM.KSDS'", 
          JSON.parse(fs.readFileSync('test/test.json')),
          (file) => {
            readUntilEnd(file, done);
          }
    );
  });

  it("open a vsam file with incorrect key length", function(done) {
    vsam( "//'BARBOZA.TEST.VSAM.KSDS'", 
          JSON.parse(fs.readFileSync('test/test-error.json')),
          (file, err) => {
            expect(file).to.be.null;
            expect(err).to.have.property('message');;
            expect(err.message).to.have.string('Incorrect key length');;
            done();
          }
    );
  });

  it("should fail to read non-existent dataset", function(done) {
    var fcn = function() {
                 vsam( "//'DOES.NOT.EXIST'", 
                 JSON.parse(fs.readFileSync('test/test.json')),
                 (file) => { assert.true(false) }
    )};
    expect(fcn).to.throw(TypeError);
    done();
  });

});
