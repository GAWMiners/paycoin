#include <boost/test/unit_test.hpp>
#include <boost/foreach.hpp>

#include "base58.h"
#include "util.h"
#include "bitcoinrpc.h"
#include "scrapesdb.h"

using namespace std;
using namespace json_spirit;

BOOST_AUTO_TEST_SUITE(rpc_tests)

static Array
createArgs(int nRequired, const char* address1=NULL, const char* address2=NULL)
{
    Array result;
    result.push_back(nRequired);
    Array addresses;
    if (address1) addresses.push_back(address1);
    if (address2) addresses.push_back(address1);
    result.push_back(addresses);
    return result;
}

BOOST_AUTO_TEST_CASE(rpc_addmultisig)
{
    rpcfn_type addmultisig = tableRPC["addmultisigaddress"]->actor;

    // old, 65-byte-long:
    const char* address1Hex = "0434e3e09f49ea168c5bbf53f877ff4206923858aab7c7e1df25bc263978107c95e35065a27ef6f1b27222db0ec97e0e895eaca603d3ee0d4c060ce3d8a00286c8";
    // new, compressed:
    const char* address2Hex = "0388c2037017c62240b6b72ac1a2a5f94da790596ebd06177c8572752922165cb4";

    Value v;
    CBitcoinAddress address;
    BOOST_CHECK_NO_THROW(v = addmultisig(createArgs(1, address1Hex), false));
    address.SetString(v.get_str());
    BOOST_CHECK(address.IsValid() && address.IsScript());

    BOOST_CHECK_NO_THROW(v = addmultisig(createArgs(1, address1Hex, address2Hex), false));
    address.SetString(v.get_str());
    BOOST_CHECK(address.IsValid() && address.IsScript());

    BOOST_CHECK_NO_THROW(v = addmultisig(createArgs(2, address1Hex, address2Hex), false));
    address.SetString(v.get_str());
    BOOST_CHECK(address.IsValid() && address.IsScript());

    BOOST_CHECK_THROW(addmultisig(createArgs(0), false), Object);
    BOOST_CHECK_THROW(addmultisig(createArgs(1), false), Object);
    BOOST_CHECK_THROW(addmultisig(createArgs(2, address1Hex), false), Object);

    BOOST_CHECK_THROW(addmultisig(createArgs(1, ""), false), Object);
    BOOST_CHECK_THROW(addmultisig(createArgs(1, "NotAValidPubkey"), false), Object);

    string short1(address1Hex, address1Hex+sizeof(address1Hex)-2); // last byte missing
    BOOST_CHECK_THROW(addmultisig(createArgs(2, short1.c_str()), false), Object);

    string short2(address1Hex+2, address1Hex+sizeof(address1Hex)); // first byte missing
    BOOST_CHECK_THROW(addmultisig(createArgs(2, short2.c_str()), false), Object);
}

// Run RPC tests over RPC...
struct RPCServerFixture
{
    RPCServerFixture() {
        SoftSetArg("-rpcuser", "paycoin_tests");
        SoftSetArg("-rpcpassword", "PAjArWaTwv8P32b13iTn1bL7aN25Y9pMmJ");
        NewThread(ThreadRPCServer, NULL);
    }

    ~RPCServerFixture() {  }
};

// Read a response object and return false if there is an error found
bool readResponse(Object obj, int &error_code) {
    // Initialize our error code as 0 because otherwise it may not have a value.
    error_code = 0;

    const Value &error = find_value(obj, "error");

    if (error.type() != null_type)
    {
        error_code = find_value(error.get_obj(), "code").get_int();
        return false;
    }

    return true;
}

Object callRPC(string strMethod, vector<string> strParams) {
    Array params = RPCConvertValues(strMethod, strParams);
    return CallRPC(strMethod, params);
}

BOOST_FIXTURE_TEST_CASE(rpc_scrapes, RPCServerFixture)
{
    // Wait a little bit to try to make sure the thread is fully started
    Sleep(100);

    /* This is a valid private key and address, do NOT use this key in a real
     * wallet, your coins will not be secure! */
    string strValidAddress = "PBMJh8s5cFzYGH6SzKJsmgrf43NcLgH47v";
    string strValidPrivKey = "U9nhhCCbFfY64wozUZ6ScrNNvBqhGtoi8cNHXmaLm3wi1VHtjhr8";
    string strValidAddress2 = "PAjArWaTwv8P32b13iTn1bL7aN25Y9pMmJ";

    // error: {"code":-4,"message":"Staking address must be in wallet."}
    string strMethod = "setscrapeaddress";
    vector<string> strParams;
    strParams.push_back(strValidAddress);
    strParams.push_back(strValidAddress2);

    Object obj = callRPC(strMethod, strParams);

    int error_code;
    BOOST_CHECK(!readResponse(obj, error_code));
    BOOST_CHECK_EQUAL(error_code, RPC_WALLET_ERROR);

    strMethod = "getscrapeaddress";
    strParams.clear();
    strParams.push_back(strValidAddress);

    obj = callRPC(strMethod, strParams);

    BOOST_CHECK(!readResponse(obj, error_code));
    BOOST_CHECK_EQUAL(error_code, RPC_WALLET_ERROR);

    strMethod = "deletescrapeaddress";

    obj = callRPC(strMethod, strParams);

    BOOST_CHECK(!readResponse(obj, error_code));
    BOOST_CHECK_EQUAL(error_code, RPC_WALLET_ERROR);

    // Import a valid private key for testing on.
    strMethod = "importprivkey";
    strParams.clear();
    strParams.push_back(strValidPrivKey);
    strParams.push_back("test");

    obj = callRPC(strMethod, strParams);

    BOOST_CHECK(readResponse(obj, error_code));

    // error: {"code":-5,"message":"Invalid Paycoin address."}
    strMethod = "setscrapeaddress";
    strParams.clear();
    strParams.push_back("PBMJh8s5cFz");
    strParams.push_back("PAjArWaTwv8");

    obj = callRPC(strMethod, strParams);

    BOOST_CHECK(!readResponse(obj, error_code));
    BOOST_CHECK_EQUAL(error_code, RPC_INVALID_ADDRESS_OR_KEY);

    strMethod = "getscrapeaddress";
    strParams.clear();
    strParams.push_back("PBMJh8s5cFz");

    obj = callRPC(strMethod, strParams);

    BOOST_CHECK(!readResponse(obj, error_code));
    BOOST_CHECK_EQUAL(error_code, RPC_INVALID_ADDRESS_OR_KEY);

    strMethod = "deletescrapeaddress";

    obj = callRPC(strMethod, strParams);

    BOOST_CHECK(!readResponse(obj, error_code));
    BOOST_CHECK_EQUAL(error_code, RPC_INVALID_ADDRESS_OR_KEY);

    // error: {"code":-5,"message":"Invalid scrape address."}
    strMethod = "setscrapeaddress";
    strParams.clear();
    strParams.push_back(strValidAddress);
    strParams.push_back("PAjArWaTwv8");

    obj = callRPC(strMethod, strParams);

    BOOST_CHECK(!readResponse(obj, error_code));
    BOOST_CHECK_EQUAL(error_code, RPC_INVALID_ADDRESS_OR_KEY);

    // error: {"code":-4,"message":"Cannot set scrape address to the same as staking address."}
    strParams.clear();
    strParams.push_back(strValidAddress);
    strParams.push_back(strValidAddress);

    obj = callRPC(strMethod, strParams);

    BOOST_CHECK(!readResponse(obj, error_code));
    BOOST_CHECK_EQUAL(error_code, RPC_WALLET_ERROR);

    // error: ("code":-4,"message":"No scrape address set for address <staking address>")
    strMethod = "getscrapeaddress";
    strParams.clear();
    strParams.push_back(strValidAddress);

    obj = callRPC(strMethod, strParams);

    BOOST_CHECK(!readResponse(obj, error_code));
    BOOST_CHECK_EQUAL(error_code, RPC_WALLET_ERROR);

    strMethod = "deletescrapeaddress";

    obj = callRPC(strMethod, strParams);

    BOOST_CHECK(!readResponse(obj, error_code));
    BOOST_CHECK_EQUAL(error_code, RPC_WALLET_ERROR);

    // Valid setscrapeaddress
    strMethod = "setscrapeaddress";
    strParams.clear();
    strParams.push_back(strValidAddress);
    strParams.push_back(strValidAddress2);

    obj = callRPC(strMethod, strParams);

    BOOST_CHECK(readResponse(obj, error_code));

    // Valid getscrapeaddress
    strMethod = "getscrapeaddress";
    strParams.clear();
    strParams.push_back(strValidAddress);

    obj = callRPC(strMethod, strParams);

    BOOST_CHECK(readResponse(obj, error_code));

    // Valid deletescrapeaddress
    strMethod = "deletescrapeaddress";

    obj = callRPC(strMethod, strParams);

    BOOST_CHECK(readResponse(obj, error_code));
}

BOOST_AUTO_TEST_SUITE_END()
