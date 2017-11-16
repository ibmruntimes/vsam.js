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
  it("create a file that doesn't exist", function(done) {
    vsam( "BARBOZA.TEST.VSAM.KSDS2", 
          JSON.parse(fs.readFileSync('test/test.json')),
          (file, err) => {
            expect(file).not.be.null;
            expect(err).to.be.null;
            expect(file.close()).to.not.throw;
            done();
          }
    );
  });

  it("open and close an existing vsam file", function(done) {
    vsam( "BARBOZA.TEST.VSAM.KSDS2", 
          JSON.parse(fs.readFileSync('test/test.json')),
          (file) => {
            expect(file).to.not.be.null;
            expect(file.close()).to.not.throw;
            done();
          }
    );
  });

 it("write new record", function(done) {
    vsam( "BARBOZA.TEST.VSAM.KSDS2", 
          JSON.parse(fs.readFileSync('test/test.json')),
          (file, err) => {
            record = {
              key: "00126",
              name: "JOHN",
              gender: "MALE"
            };
            file.write(record, (err) => {
              assert.ifError(err);
              expect(file.close()).to.not.throw;
              done();
            });
          });
  });

  it("reads one record and verifies it's properties", function(done) {
    vsam( "BARBOZA.TEST.VSAM.KSDS2", 
          JSON.parse(fs.readFileSync('test/test.json')),
          (file) => {
            file.read( (record, err) => {
              assert.ifError(err);
              expect(record).to.not.be.null;
              expect(record).to.have.property('key');
              expect(record).to.have.property('name');
              expect(record).to.have.property('gender');
              expect(file.close()).to.not.throw;
              done();
            });
          });
  });

  it("find existing record and verify data", function(done) {
    vsam( "BARBOZA.TEST.VSAM.KSDS2", 
          JSON.parse(fs.readFileSync('test/test.json')),
          (file, err) => {
            file.find("00126", (record, err) => {
              assert.ifError(err);
              assert.equal(record.key, "00126", "record has been created");
              assert.equal(record.name, "JOHN", "created record has correct name");
              assert.equal(record.gender, "MALE", "created record has correct gender");
              expect(file.close()).to.not.throw;
              done();
            });
          });
  });

  it("write new record after read", function(done) {
    vsam( "BARBOZA.TEST.VSAM.KSDS2", 
          JSON.parse(fs.readFileSync('test/test.json')),
          (file, err) => {
            file.read((record, err) => {
              record.key = "00125";
              record.name = "JANE";
              record.gender = "FEMALE";
              file.write(record, (err) => {
                assert.ifError(err);
                file.find("00125", (record, err) => {
                  assert.ifError(err);
                  assert.equal(record.key, "00125", "record has been created");
                  assert.equal(record.name, "JANE", "created record has correct name");
                  assert.equal(record.gender, "FEMALE", "created record has correct gender");
                  expect(file.close()).to.not.throw;
                  done();
                });
              });
            });
          });
  });

  it("delete existing record", function(done) {
    vsam( "BARBOZA.TEST.VSAM.KSDS2", 
          JSON.parse(fs.readFileSync('test/test.json')),
          (file, err) => {
            assert.ifError(err);
            file.find("00126", (record, err) => {
              file.delete( (err) => {
                assert.ifError(err);
                file.find("00126", (err) => {
                  assert.ifError(err);
                  expect(file.close()).to.not.throw;
                  done();
                });
              });
            });
          });
  });

  it("reads all records until the end", function(done) {
    vsam( "BARBOZA.TEST.VSAM.KSDS2", 
          JSON.parse(fs.readFileSync('test/test.json')),
          (file) => {
            readUntilEnd(file, done);
          }
    );
  });

  it("open a vsam file with incorrect key length", function(done) {
    vsam( "BARBOZA.TEST.VSAM.KSDS2", 
          JSON.parse(fs.readFileSync('test/test-error.json')),
          (file, err) => {
            expect(file).to.be.null;
            expect(err).to.have.property('message');;
            expect(err.message).to.have.string('Incorrect key length');;
            done();
          });
  });

  it("throws exception for non-existent dataset", function(done) {
    var fcn = function() {
                 vsam( "//'A'", 
                 JSON.parse(fs.readFileSync('test/test.json')),
                 (file) => { assert.true(false) }
    )};
    expect(fcn).to.throw(TypeError);
    done();
  });

  it("update existing record and delete it", function(done) {
    vsam( "BARBOZA.TEST.VSAM.KSDS2", 
          JSON.parse(fs.readFileSync('test/test.json')),
          (file, err) => {
            file.find("00125", (record, err) => {
              assert.ifError(err);
              record.name = "KEVIN";
              record.gender = "MALE";
              file.update(record, (err) => {
                assert.ifError(err);
                file.find("00125", (record, err) => {
                  assert.ifError(err);
                  assert.equal(record.name, "KEVIN", "name has been updated");
                  assert.equal(record.gender, "MALE", "gender has been updated");
                  file.delete( (err) => {
                    assert.ifError(err);
                    expect(file.close()).to.not.throw;
                    done();
                   });
                });
              });
            });
          });
  });

});
