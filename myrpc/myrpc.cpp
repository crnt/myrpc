// myrpc.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

class session;
class my_socket {};
struct data_type {
    int i;
    float f;
};

typedef int session_id_type;
typedef boost::shared_future<data_type> future_data;

class callable {
public:
    callable(session_id_type id, const future_data& future_result, const boost::shared_ptr<session>& session) : 
      id(id), f(future_result), weak_session_ptr(session)
      {}
    ~callable();

    future_data& get_future() { return f; }
    data_type get_data() { 
        return f.get(); 
    }


protected:
    session_id_type id;
    future_data f;
    boost::weak_ptr<session> weak_session_ptr;
};

class session : public boost::enable_shared_from_this<session> {
public:
    session() : current_id(0) {}

    callable create_call();
    void remove_unused_callable(session_id_type id, bool reset_data);

protected:
    session_id_type current_id;
    typedef boost::shared_ptr<boost::promise<data_type> > promise_type;
    typedef std::map<session_id_type, promise_type> promise_map_type;
    my_socket s;

    typedef boost::recursive_mutex mutex_type;
    mutex_type mutex;
    promise_map_type promise_map;
};

callable::~callable()
{
    if(boost::shared_ptr<session> r = weak_session_ptr.lock())
    {
        r->remove_unused_callable(id, !f.has_value());
    }
}

void session::remove_unused_callable(session_id_type id, bool reset_data)
{
    mutex_type::scoped_lock lock(mutex);

    // There is no clients for this 'promise',
    // so, we can setup it to default value before deleting.
    // The only reason to do it is calm debuggers that detect throwning exception in boost library
    if (reset_data)
        promise_map[id]->set_value(data_type());
    promise_map.erase(id);
}

callable session::create_call()
{
    mutex_type::scoped_lock lock(mutex);

    // create new future
    session_id_type id = current_id++;
    promise_type new_promise(new boost::promise<data_type>);
    promise_map[id] = new_promise;
    future_data f(new_promise->get_future());
    return callable(id, f, shared_from_this());
}

int main()
{
    boost::asio::io_service io;
    boost::thread t(boost::bind(&boost::asio::io_service::run, &io));

    boost::shared_ptr<session> s(new session);
    callable call = s->create_call();
    future_data& f = call.get_future();
    // f.get(); // wait until result

    t.join();
    return 0;
}
