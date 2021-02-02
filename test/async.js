/*
 * Licensed Materials - Property of IBM
 * (C) Copyright IBM Corp. 2017. All Rights Reserved.
 * US Government Users Restricted Rights - Use, duplication or disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
*/

const vsam = require("../build/Release/vsam.js.node");
const async = require('async');
const fs = require('fs');
const expect = require('chai').expect;
const should = require('chai').should;
const assert = require('chai').assert;
const { execSync } = require('child_process');

// Set IBM_VSAM_UID environment variable for customer uid
const uid = process.env.IBM_VSAM_UID || execSync('whoami').slice(0, -1);
const testSet = `${uid}.TEST2.VSAM.KSDS2`;
const lmt =  process.version.split('.')[0] === 'v12' ? " - see FIXME re delay with the 2 promises tests" : "";
const title = `VSAM Key Sequenced Dataset - async APIs${lmt}`

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

describe(title, function() {
  before(function() {
    if (vsam.exist(testSet)) {
      var file = vsam.openSync(testSet,
          JSON.parse(fs.readFileSync('test/schema.json')));
      expect(file.close()).to.not.throw;
      file.dealloc((err) => {
        assert.ifError(err);
      });
    }
  });

  it("ensure test dataset does not exist", function(done) {
    expect(vsam.exist(testSet)).to.be.false;
    done();
  });

  it("create an empty dataset", function(done) {
    var file = vsam.allocSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    expect(file).not.be.null;
    expect(file.close()).to.not.throw;
    done();
  });

  it("ensure test dataset exists", function(done) {
    expect(vsam.exist(testSet)).to.be.true;
    done();
  });

  it("open and close the existing dataset", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    expect(file).to.not.be.null;
    expect(file.close()).to.not.throw;
    done();
  });

  it("verify error from I/O calls on a closed dataset", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    expect(file.close()).to.not.throw;
    var record = { key: "1234", name: "diana", amount: "1234" }
    const keybuf = Buffer.from([0x01, 0x02, 0x03, 0x04]);
    file.read((record, err) => { assert.equal(err, "read error: VSAM dataset is not open."); });
    file.find("1234", (record, err) => { assert.equal(err, "find error: VSAM dataset is not open."); });
    file.findeq("1234", (record, err) => { assert.equal(err, "find error: VSAM dataset is not open."); });
    file.findge("1234", (record, err) => { assert.equal(err, "findge error: VSAM dataset is not open."); });
    file.findfirst((record, err) => { assert.equal(err, "findfirst error: VSAM dataset is not open."); });
    file.findlast((record, err) => { assert.equal(err, "findlast error: VSAM dataset is not open."); });
    file.update(record, (err) => { assert.equal(err, "update error: VSAM dataset is not open."); });
    file.update("f1f2", record, (count, err) => { assert.equal(err, "update error: VSAM dataset is not open."); });
    file.update(keybuf, keybuf.length, record, (count, err) => { assert.equal(err, "update error: VSAM dataset is not open."); });
    file.delete((err) => { assert.equal(err, "delete error: VSAM dataset is not open."); });
    file.delete("f1f2", (count, err) => { assert.equal(err, "delete error: VSAM dataset is not open."); });
    file.delete(keybuf, keybuf.length, (count, err) => { assert.equal(err, "delete error: VSAM dataset is not open."); });
    file.write(record, (err) => { assert.equal(err, "write error: VSAM dataset is not open."); });
    done();
  });

  it("return error for opening empty dataset in read-only mode", function(done) {
    expect(() => {
      vsam.openSync(testSet,
                    JSON.parse(fs.readFileSync('test/schema.json')),
                    'rb,type=record');
    }).to.throw(/open error: fopen\(\) failed: EDC5041I An error was detected at the system level when opening a file. \(R15=8, errno2=0xc00a0022\)./);

    done();
  });

  it("verify error from zero-length key buffer", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    const keybuf = Buffer.from([]);
    file.find(keybuf, keybuf.length, (record, err) => {
      assert.equal(err, "find error: length of 'key' must be 1 or more.");
      expect(file.close()).to.not.throw;
      done();
    });
  });

  it("verify error from zero-length key string", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    file.find("", (record, err) => {
      assert.equal(err, "find error: length of 'key' must be 1 or more.");
      expect(file.close()).to.not.throw;
    });
    done();
  });

  it("verify error from hexadecimal key buffer exceeding max length", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    const keybuf = Buffer.from([0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0xa7, 0xb8, 0x00]);
    file.find(keybuf, keybuf.length, (record, err) => {
      assert.equal(err, "find error: length 9 of 'key' exceeds schema's length 8.");
      expect(file.close()).to.not.throw;
      done();
    });
  });

  it("verify error from hexadecimal key string exceeding max length", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    file.find("0xa1b2c3d4e5f6a7a800", (record, err) => {
      assert.equal(err, "find error: number of hex digits 9 for 'key' exceed schema's length 8, found <0xa1b2c3d4e5f6a7a800>.");
      expect(file.close()).to.not.throw;
      done();
    });
  });

  it("verify error from hexadecimal key string containing non-hex", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    file.find("0xa1g2c3d4e5f6a7a800", (record, err) => {
      assert.equal(err, "find error: hex string for 'key' must contain only hex digits 0-9 and a-f or A-F, with an optional 0x prefix, found <0xa1g2c3d4e5f6a7a800>.");
      expect(file.close()).to.not.throw;
      done();
    });
  });

  it("verify error from hexadecimal key string prefixed with invalid 0y", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    file.find("0ya1b2c3d4e5f6a7a800", (record, err) => {
      assert.equal(err, "find error: hex string for 'key' must contain only hex digits 0-9 and a-f or A-F, with an optional 0x prefix, found <0ya1b2c3d4e5f6a7a800>.");
      expect(file.close()).to.not.throw;
      done();
    });
  });

  it("verify error from write record with hexadecimal key string exceeding max length", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    const keybuf = Buffer.from([0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0xa7, 0xb8, 0x00]);
    record = {
      key: keybuf.toString('hex'),
      name: "JOHN",
      amount: "1234"
    };
    file.write(record, (err) => {
      assert.equal(err, "write error: number of hex digits 9 for 'key' exceed schema's length 8, found <a1b2c3d4e5f6a7b800>.");
      expect(file.close()).to.not.throw;
      done();
    });
  });

  it("verify error from write record with a field exceeding max length", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    const keybuf = Buffer.from([0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0xa7, 0xb8]);
    record = {
      key: keybuf.toString('hex'),
      name: "exceed by 1",
      amount: "1234"
    };
    file.write(record, (err) => {
      assert.equal(err, "write error: length 11 of 'name' exceeds schema's length 10.");
      expect(file.close()).to.not.throw;
      done();
    });
  });

  it("write record on empty dataset and find it, write same record again and verify error", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    const keybuf = Buffer.from([0xa1, 0xb2, 0xc3, 0xd4]);
    record = {
      key: keybuf.toString('hex'),
      name: "JOHN",
      amount: "1234"
    };
    file.write(record, (err) => {
      assert.ifError(err);
      file.find(keybuf, keybuf.length, (record, err) => {
        assert.ifError(err);
        assert.equal(record.key, "a1b2c3d4", "record has been created");
        assert.equal(record.name, "JOHN", "created record has correct name");
        assert.equal(record.amount, "1234", "created record has correct amount");

        file.write(record, (err) => {
          assert.equal(err, "write error: an attempt was made to store a record with a duplicate key: EDC5065I A write system error was detected. (R15=8, errno2=0xc0500091).");
          expect(file.close()).to.not.throw;
          done();
        });
      });
    });
  });

  it("write another new record, provide key as Buffer", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
// not supported yet:
//    key: Buffer.from([0xe5, 0xf6, 0x78, 0x9a, 0xfa, 0xbc, 0xd]),
//    key: Buffer.from([0xe5, 0xf6, 0x78, 0x9a, 0xfa, 0xbc, 0xd]).toString('hex'),
    record = {
      // trailing 00 will be truncated when read
      key: "e5f6789afabcd000",
      name: "JIM",
      amount: "9876543210"
    };
    file.write(record, (err) => {
      assert.ifError(err);
      expect(file.close()).to.not.throw;
      done();
    });
  });

  it("verify error from updating a record that results in a duplcate key", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    file.find("e5f6789afabcd000", (record, err) => {
      assert.ifError(err);
      record.key = "a1b2c3d4"
      file.update(record, (err) => {
        assert.equal(err, "update error: an attempt was made to store a record with a duplicate key: EDC5065I A write system error was detected. (R15=8, errno2=0xc0500090).");
        expect(file.close()).to.not.throw;
        done();
      });
    });
  });

  it("verify update with value length less than minLength specified", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    file.find("e5f6789afabcd000", (record, err) => {
      assert.ifError(err);
      record.name = "";
      file.update(record, (err) => {
        assert.equal(err, "update error: length of 'name' must be 1 or more.");
        expect(file.close()).to.not.throw;
        done();
      });
    });
  });

  it("read a record and verify properties", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    file.read( (record, err) => {
      assert.ifError(err);
      expect(record).to.not.be.null;
      expect(record).to.have.property('key');
      expect(record).to.have.property('name');
      expect(record).to.have.property('amount');
      expect(file.close()).to.not.throw;
      done();
    });
  });

  it("verify find, findlast, findfirst, findge followed by a bunch of write/update/find/delete", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    const keybuf = Buffer.from([0xa1, 0xb2, 0xc3, 0xd4]);
    file.find(keybuf, keybuf.length, (record, err) => {
      assert.ifError(err);
      assert.equal(record.key, "a1b2c3d4", "record has been created");
      assert.equal(record.name, "JOHN", "created record has correct name");
      assert.equal(record.amount, "1234", "created record has correct amount");

      file.findlast((record, err) => {
        assert.ifError(err);
        assert.equal(record.key, "e5f6789afabcd0", "record has been created (trailing 0 retained)");
        assert.equal(record.name, "JIM", "created record has correct name");
        assert.equal(record.amount, "9876543210", "created record has correct amount");

        file.findfirst((record, err) => {
          assert.ifError(err);
          assert.equal(record.key, "a1b2c3d4", "record has been created");
          assert.equal(record.name, "JOHN", "created record has correct name");
          assert.equal(record.amount, "1234", "created record has correct amount");

          file.findge("43b2c3d0", (record, err) => {
            assert.ifError(err);
            assert.equal(record.key, "a1b2c3d4", "record has been created");
            assert.equal(record.name, "JOHN", "created record has correct name");
            assert.equal(record.amount, "1234", "created record has correct amount");

            record.key = "F1F2F3F4";
            record.name = "TEST ";
            file.write(record, (err) => {
              assert.ifError(err);

              file.find("F1F2F3F4", (record, err) => {
                assert.ifError(err);
                assert.equal(record.name, "TEST ", "created record has correct name");
                record.name = "U  ";
                file.update(record, (err) => {
                  assert.ifError(err);
                  assert.equal(record.name, "U  ", "created record has correct name");

                  const keybuf = Buffer.from([0xc1, 0xd2, 0xe3, 0xf4]);
                  record = {
                    key: keybuf.toString('hex'),
                    name: "MARY",
                    amount: "999"
                  };
                  file.write(record, (err) => {
                    assert.ifError(err);

                    file.find(keybuf, keybuf.length, (record, err) => {
                      assert.ifError(err);
                      assert.equal(record.name, "MARY", "created record has correct name");
                      record.name = "Mary Smith";
                      file.update(record, (err) => {
                        assert.ifError(err);
                        assert.equal(record.name, "Mary Smith", "created record has correct name");

                        file.find(keybuf, keybuf.length, (record, err) => {
                          assert.ifError(err);
                          assert.equal(record.name, "Mary Smith", "created record has correct name");

                          file.delete( (err) => {
                            assert.ifError(err);

                            file.find("0xF1F2F3F4", (record, err) => {
                              assert.ifError(err);
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
                });
              });
            });
          });
        });
      });
    });
  });

  it("write new record after read", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    file.read((record, err) => {
      // trailing 00 will be truncated, key up to 'c' matches JIM's, whose key is then followed by 'd'
      record.key = "e5f6789afabc00";
      record.name = "JANE";
      record.amount = "4187832145";
      file.write(record, (err) => {
        assert.ifError(err);
        file.find("e5f6789afabc", (record, err) => {
          assert.ifError(err);
          assert.equal(record.key, "e5f6789afabc", "record has been created (00 truncated)");
          assert.equal(record.name, "JANE", "created record has correct name");
          assert.equal(record.amount, "4187832145", "created record has correct amount");
          expect(file.close()).to.not.throw;
          done();
        });
      });
    });
  });

  it("write new record with max fields' lengths including 0's", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    file.read((record, err) => {
      record.key = "A900B2F4fabc00e9";
      record.name = "M Name TCD";
      record.amount = "A1b2c3D4F6000090";
      file.write(record, (err) => {
        assert.ifError(err);
        file.find("a900b2F4fabc00E9", (record, err) => {
          assert.ifError(err);
          assert.equal(record.key, "a900b2f4fabc00e9", "record has been created");
          assert.equal(record.name, "M Name TCD", "created record has correct name");
          assert.equal(record.amount, "a1b2c3d4f6000090", "created record has correct amount");
          expect(file.close()).to.not.throw;
          done();
        });
      });
    });
  });

  //
  // FIXME(gabylb): even though all 4 promises resolve/reject in the expected time
  // (and DONE-* displayed), it may take too long (~30 seconds) for Promise.all() to return.
  // This happens only when the block below is run in mocha and Node version v12.
  // With Node v8, those test time out, but pass when run outside of mocha.

  if (process.version.split('.')[0] !== 'v8') {
   it("run multiple read-only transactions async", function() {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')),
                             "rb,type=record");
    function p1() {
      return new Promise((resolve, reject) => {
        file.find("A900B2F4fabc00e9", (record, err) => {
          //console.log('DONE-find-1, err=' + err);
          assert.ifError(err);
          assert.equal(record.key, "a900b2f4fabc00e9", "6. record has been created");
          assert.equal(record.name, "M Name TCD", "5. created record has correct name");
          assert.equal(record.amount, "a1b2c3d4f6000090", "6. created record has correct amount");
          resolve(0);
        })
      })
    }
    function p2() {
      return new Promise((resolve, reject) => {
        file.findlast((record, err) => {
          //console.log('DONE-find-2, err=' + err);
          assert.ifError(err);
          assert.equal(record.key, "e5f6789afabcd0", "2. record has been created (trailing 0 retained)");
          assert.equal(record.name, "JIM", "created record has correct name");
          assert.equal(record.amount, "9876543210", "created record has correct amount");
          resolve(0);
        })
      })
    }
    function p3() {
      return new Promise((resolve, reject) => {
        file.findfirst((record, err) => {
          //console.log('DONE-find-3, err=' + err);
          assert.ifError(err);
          assert.equal(record.key, "a1b2c3d4", "3. record has been created");
          assert.equal(record.name, "JOHN", "created record has correct name");
          assert.equal(record.amount, "1234", "created record has correct amount");
          resolve(0);
        })
      })
    }
    function p4() {
      return new Promise((resolve, reject) => {
        file.findge("43b2c3d0", (record, err) => {
          //console.log('DONE-find-4, err=' + err);
          assert.ifError(err);
          assert.equal(record.key, "a1b2c3d4", "4. record has been created");
          assert.equal(record.name, "JOHN", "created record has correct name");
          assert.equal(record.amount, "1234", "created record has correct amount");
          resolve(0);
        })
      })
    }
    return Promise.all([p1(),p2(),p3(),p4()]).then(res => {
      //console.log('DONE-find-all: OK');
      expect(file.close()).to.not.throw;
    }).catch(err => {
      //console.log("DONE-find-all: ERROR=" + err);
      assert.ifError(err);
      expect(file.close()).to.not.throw;
    })
   });
  }

  it("delete existing record, then find it and verify error, then delete it and verify error", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
//  const keybuf = Buffer.from([0xa1, 0xb2, 0xc3, 0xd4]);
    const keybuf = Buffer.from([0xA1, 0xB2, 0xc3, 0xD4]);
    file.find(keybuf, keybuf.length, (record, err) => {
      assert.ifError(err);
      assert.equal(record.key, "a1b2c3d4", "record has been created");
      assert.equal(record.name, "JOHN", "created record has correct name");
      assert.equal(record.amount, "1234", "created record has correct amount");
      file.delete( (err) => {
        assert.ifError(err);
        file.find("a1b2c3d4", (record, err) => {
          assert.equal(err, "no record found", "correct error message"); 
          assert.equal(record, null);

          file.delete("a1b2c3d4", (count, err) => {
            assert.equal(err, "no record found with the key for delete");
            assert.equal(count, 0);
            expect(file.close()).to.not.throw;
            done();
          });
        });
      });
    });
  });

  it("reads all records until the end", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    readUntilEnd(file, done);
  });

  it("open a vsam file with incorrect key length", function(done) {
    expect(() => {
      vsam.openSync(testSet,
                    JSON.parse(fs.readFileSync('test/schema-error.json')));
    }).to.throw(/open error: key length 8 doesn't match length 6 in schema./);
    done();
  });

  it("return error for accessing a dataset that doesn't exist", function(done) {
    expect(() => {
      vsam.openSync("A9y8o2.X", // test will fail if it actually exists and user can access it
                    JSON.parse(fs.readFileSync('test/schema.json')));
    }).to.throw(/open error: fopen\(\) failed: EDC5049I The specified file name could not be located. \(R15=2, errno2=0xc00b0641\)./);
    done();
  });

  it("return error for invalid dataset name (invalid character)", function(done) {
    expect(() => {
      const testSet = `${uid}.A.B._`;
      vsam.openSync(testSet,
                    JSON.parse(fs.readFileSync('test/schema.json')));
    }).to.throw(/open error: fopen\(\) failed: EDC5047I An invalid file name was specified as a function parameter. \(R15=0, errno2=0xc00b0287\)./);
    done();
  });

  it("return error for invalid dataset name (qualifier length < 1)", function(done) {
    expect(() => {
      vsam.openSync("A..B",
                    JSON.parse(fs.readFileSync('test/schema.json')));
    }).to.throw(/open error: fopen\(\) failed: EDC5047I An invalid file name was specified as a function parameter. \(R15=0, errno2=0xc00b0286\)./);
    done();
  });

  it("return error for invalid dataset name (qualifier length > 8)", function(done) {
    expect(() => {
      vsam.openSync("A.ABCDEFGHI.B",
                    JSON.parse(fs.readFileSync('test/schema.json')));
    }).to.throw(/open error: fopen\(\) failed: EDC5047I An invalid file name was specified as a function parameter. \(R15=0, errno2=0xc00b0286\)./);
    done();
  });

  it("open in read-only mode, reads all records until the end", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')),
                             "rb,type=record");
    readUntilEnd(file, done);
  });

  it("update existing record and delete it", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    file.find("e5f6789afabc", (record, err) => {
      assert.ifError(err);
      record.name = "KEVIN";
      record.amount = "678123";
      file.update(record, (err) => {
        assert.ifError(err);
        file.find("e5f6789afabc", (record, err) => {
          assert.ifError(err);
          assert.equal(record.name, "KEVIN", "name was not updated");
          assert.equal(record.amount, "678123", "amount was not updated");
          file.delete( (err) => {
            assert.ifError(err);
            expect(file.close()).to.not.throw;
            done();
          });
        });
      });
    });
  });

  it("leave a record field undefined for write and update", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    record = {
      key: "F1F2F3F4",
      name: "TEST"
      // leave amount undefined, should be set to all 0x00
    };
    file.write(record, (err) => {
      assert.ifError(err);
      file.find(record.key, (record2, err) => {
        assert.ifError(err);
        assert.equal(record2.key, "f1f2f3f4", "correct key in record");
        assert.equal(record2.name, "TEST", "correct name in record");
        assert.equal(record2.amount, "00", "default amount set to 0 as expected");

        record3 = {
          key: "F1F2F3F4",
          name: "TEST"
          // leave amount undefined, update should fail
        };
        file.update(record3, (err) => {
          assert.equal(err, "update error: update value for amount has not been set.");

          record4 = {
            key: "F1F2F3F4",
            amount: "0x1234"
            // leave name undefined, write should fail because minLength=1
          };
          file.write(record4, (err) => {
            assert.equal(err, "write error: length of 'name' must be 1 or more.");
            expect(file.close()).to.not.throw;
            done();
          });
        });
      });
    });
  });

  it("find and update in one call, also verify no record found error", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    record = {
      // leave key and name undefined, so only amount is updated in the found record
      amount: "0123456789000198"
    };
    file.update("f1f2f3f4", record, (count, err) => {
      assert.ifError(err);
      assert.equal(count, 1);
      file.update("f1f2f3f499", record, (count, err) => {
        assert.equal(err, "no record found with the key for update");
        assert.equal(count, 0);
        expect(file.close()).to.not.throw;
        done();
      });
    });
  });

  it("find and update in one call, also verify no record found error", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    // verify record previously updated after file close/open
    file.find("f1f2f3f4", (record, err) => {
      assert.equal(record.key, "f1f2f3f4", "correct key");
      assert.equal(record.name, "TEST", "correct name");
      assert.equal(record.amount, "0123456789000198", "correct amount");

      // find and update another record with key-buf, key-buflen:
      record2 = {
        name: "diane",
        amount: "12"
      };
      const keybuf = Buffer.from([0xA9, 0, 0xB2, 0xF4, 0xfa, 0xbc, 0, 0xe9]);
      file.update(keybuf, keybuf.length, record2, (count, err) => {
        assert.ifError(err);

        // independant test (of above) for no record found
        file.update("f1f2f3f4f5", record, (count, err) => {
          assert.equal(err, "no record found with the key for update");
          assert.equal(count, 0);
          expect(file.close()).to.not.throw;
          done();
        });
      });
    });
  });

  it("find and delete in one call", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    file.delete("e5f6789afabc", (count, err) => {
      assert.ifError(err);
      assert.equal(count, 1);
      expect(file.close()).to.not.throw;
      done();
    });
  });

  it("find and delete in one call, also verify no record found error", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    // verify record previously deleted with find-delete after file close/open
    file.find("e5f6789afabc", (record, err) => {
      assert.equal(err, "no record found");
      assert.equal(record, null);

      const keybuf = Buffer.from([0xe5, 0xf6, 0x78, 0x9a, 0xfa, 0xbc, 0xd0]);
      file.delete(keybuf, keybuf.length, (count, err) => {
        assert.equal(count, 0);

        // find and delete another record with key-string:
        file.delete("f1f2f3f4", (count, err) => {
          assert.ifError(err);
          assert.equal(count, 1);

          // independant test (of above) for no record found
          file.delete("f1f2f3f4f5f6", (count, err) => {
            assert.equal(err, "no record found with the key for delete");
            assert.equal(count, 0);
            expect(file.close()).to.not.throw;
            done();
          });
        });
      });
    });
  });

  it("reads all records until the end", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    readUntilEnd(file, done);
  });

  it("write new records to test multiple find-update and find-delete", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    record = { key: "f1f2f3f4f5f6f7f8", name: "name 12341", amount: "f1f2f3f4f5f6f7f8" };
    file.write(record, (err) => {
      assert.ifError(err);
      record = { key: "a1a2a3a4a5a6a7a8", name: "name 12341", amount: "a1f2f3f4f5f6f7f8" };
      file.write(record, (err) => {
        assert.ifError(err);
        record = { key: "a1a2a3a4b5b6b7b8", name: "name 2342", amount: "b1b2b3b4f5f6f7f8" };
        file.write(record, (err) => {
          assert.ifError(err);
          record = { key: "c1c2c3c4a5a6a7a8", name: "name 32343", amount: "b1b2b3f4c5f6f7f8" };
          file.write(record, (err) => {
            assert.ifError(err);
            record = { key: "b1b2b3b4c5b6b7b8", name: "name 42344", amount: "c1c2c3c4f5f6f7f8" };
            file.write(record, (err) => {
              assert.ifError(err);
              record = { key: "a1a2a3a4", name: "name 52345", amount: "d1d2d3f4f5f6f7f8" };
              file.write(record, (err) => {
                assert.ifError(err);
                record = { key: "a1a2a3a4b5b6b7", name: "name 62", amount: "e1b2b3b4f5f6f7f8" };
                file.write(record, (err) => {
                  assert.ifError(err);
                  record = { key: "c1c2c3c4a5a", name: "name 72347", amount: "f1a2a3f4c5" };
                  file.write(record, (err) => {
                    assert.ifError(err);
                    record = { key: "b1b2b3b4", name: "name 82348", amount: "c1c2c3c4f5f6f7f8" };
                    file.write(record, (err) => {
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
      });
    });
  });

  it("write more new records to test multiple find-update and find-delete", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    record = { key: "f1f2f3f4", name: "name 12341", amount: "" };
    file.write(record, (err) => {
      assert.ifError(err);
      record = { key: "f1f2f3f4f5f6f7", name: "NAME 12341", amount: "a1f2f3f400f6f7" };
      file.write(record, (err) => {
        assert.ifError(err);
        record = { key: "f1f2f3f4b5b6b7b8", name: "NAME 2342", amount: "00b3b4f5f6f7f8" };
        file.write(record, (err) => {
          assert.ifError(err);
          record = { key: "c2c3c4a5a6a7a8", name: "NAME 32343", amount: "00b2b3f4c5f6f7f8" };
          file.write(record, (err) => {
            assert.ifError(err);
            record = { key: "b3b4c5b6b7b8", name: "NAME 42344", amount: "c3c4f5f6f7f8" };
            file.write(record, (err) => {
              assert.ifError(err);
              record = { key: "a1", name: "NAME 52345", amount: "d1d2d3f4f5f6f7f8" };
              file.write(record, (err) => {
                assert.ifError(err);
                record = { key: "a2", name: "NAME 62", amount: "e1b2b3b4f5f6f7f8" };
                file.write(record, (err) => {
                  assert.ifError(err);
                  record = { key: "a3", name: "NAME 72347", amount: "f1a2a3f4c5" };
                  file.write(record, (err) => {
                    assert.ifError(err);
                    record = { key: "b1b2", name: "NAME 82348", amount: "c3c4f5f6f7f8" };
                    file.write(record, (err) => {
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
      });
    });
  });

  it("test updating and deleting multiple records", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    record = { name: "12341 nam", amount: "00234567890abcde" };
    file.update("f1f2f3f4", record, (count, err) => {
      assert.ifError(err);
      assert.equal(count, 4);

      file.update("a100000000000000", record, (count, err) => {
        assert.ifError(err);
        assert.equal(count, 1);

        file.delete("a1a2a3a4b5", (count, err) => {
          assert.ifError(err);
          assert.equal(count, 2);

          file.update("a1a2a3a4", record, (count, err) => {
            assert.ifError(err);
            assert.equal(count, 2);

            file.find("a1a2a3a4", (record, err) => {
              assert.ifError(err);
              assert.equal(record.key, "a1a2a3a4");
              assert.equal(record.name, "12341 nam");
              assert.equal(record.amount, "00234567890abcde");

              file.read( (record, err) => {
                assert.ifError(err);
                assert.equal(record.key, "a1a2a3a4a5a6a7a8");
                assert.equal(record.name, "12341 nam");
                assert.equal(record.amount, "00234567890abcde");

                file.delete("b1b2", (count, err) => {
                  assert.ifError(err);
                  assert.equal(count, 3);
                  file.find("b1b2", (record, err) => {
                    assert.equal(err, "no record found");

                    file.delete("b3b4c5b6b7b8", (count, err) => {
                      assert.ifError(err);
                      assert.equal(count, 1);

                      file.delete("f1f2f3f4", (count, err) => {
                        assert.ifError(err);
                        assert.equal(count, 4);

                        file.delete("a1", (count, err) => {
                          assert.ifError(err);
                          assert.equal(count, 3);

                          file.delete("c1", (count, err) => {
                            assert.ifError(err);
                            assert.equal(count, 2);

                            file.delete("a2", (count, err) => {
                              assert.ifError(err);
                              assert.equal(count, 1);
                              expect(file.close()).to.not.throw;
                              done();
                            });
                          });
                        });
                      });
                    });
                  });
                });
              });
            });
          });
        });
      });
    });
  });

  it("test update error during validation followed by a valid update", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    record = { amount: "123" };
    file.update("f1f2f3f4f5f6f7f8f9", record, (count, err) => {
      assert.equal(count, 0);
      assert.equal(err, "update error: number of hex digits 9 for 'key' exceed schema's length 8, found <f1f2f3f4f5f6f7f8f9>.");

      file.update("a900b2f4fabc00e9", record, (count, err) => {
        assert.equal(count, 1);
        assert.equal(err, null);
        expect(file.close()).to.not.throw;
        done();
      });
    });
  });

  it("reads all records until the end", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')),
                             'rb,type=record');
    readUntilEnd(file, done);
  });

  // FIXME(gabylb): even though all 3 promises resolve/reject in the expected time
  // (and DONE-* displayed), it may take too long (~30 seconds) for Promise.all() to return.
  // This happens only when the block below is run in mocha and Node version v12.
  // With Node v8, those test time out, but pass when run outside of mocha.
  //

  if (process.version.split('.')[0] !== 'v8') {
   it("run multiple find-update transactions async", function() {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));

    function p1() {
      return new Promise((resolve, reject) => {
        record = { name: "UPD c2c3 N", amount: "c5f6f7f800b2b3f4" };
        file.update("c2c3c4a5a6a7a8", record, (count, err) => {
          //console.log('DONE-update-1, err=' + err);
          assert.ifError(err);
          assert.equal(count, 1);
          resolve(0);
        })
      })
    }
    function p2() {
      return new Promise((resolve, reject) => {
        record = { name: "diana", amount: "c5c4c3c2c1c" };
        file.update("a900", record, (count, err) => {
          //console.log('DONE-update-2, err=' + err);
          assert.ifError(err);
          assert.equal(count, 1);
          resolve(0);
        })
      })
    }
    function p3() {
      return new Promise((resolve, reject) => {
        file.delete("a3", (count, err) => {
          //console.log('DONE-delete, err=' + err);
          assert.ifError(err);
          assert.equal(count, 1);
          resolve(0);
        })
      })
    }
    return Promise.all([p1(),p2(),p3()]).then(res => {
      //console.log('DONE-update-delete-all: OK');
      expect(file.close()).to.not.throw;
    }).catch(err => {
      //console.log('DONE-update-delete-all: ERROR=' + err);
      assert.ifError(err);
      expect(file.close()).to.not.throw;
    })
   });

   it("write back a records to enable above test run in a loop if needed", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    record = { key: "a3", name: "NAME 72347", amount: "f1a2a3f4c5" };
    file.write(record, (err) => {
      assert.ifError(err);
      expect(file.close()).to.not.throw;
      done();
    });
   });
}

  it("reads all records until the end", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')),
                             'rb,type=record');
    readUntilEnd(file, done);
  });

  it("deallocate a dataset", function(done) {
    var file = vsam.openSync(testSet,
                             JSON.parse(fs.readFileSync('test/schema.json')));
    expect(file.close()).to.not.throw;
    file.dealloc((err) => {
      assert.ifError(err);
      done();
    });
  });
});
