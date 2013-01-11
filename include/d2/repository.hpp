/**
 * This file defines the `Repository` class.
 */

#ifndef D2_REPOSITORY_HPP
#define D2_REPOSITORY_HPP

#include <d2/detail/exceptions.hpp>
#include <d2/sandbox/container_view.hpp>

#include <boost/assert.hpp>
#include <boost/filesystem.hpp>
#include <boost/fusion/include/all.hpp>
#include <boost/fusion/include/as_map.hpp>
#include <boost/fusion/include/at_key.hpp>
#include <boost/fusion/include/for_each.hpp>
#include <boost/fusion/include/mpl.hpp>
#include <boost/fusion/include/pair.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/mpl/apply.hpp>
#include <boost/mpl/bool.hpp>
#include <boost/mpl/transform.hpp>
#include <boost/operators.hpp>
#include <boost/optional.hpp>
#include <boost/phoenix/core.hpp>
#include <boost/phoenix/operator.hpp>
#include <boost/type_traits/remove_reference.hpp>
#include <boost/unordered_map.hpp>
#include <boost/utility/result_of.hpp>
#include <boost/utility/typed_in_place_factory.hpp>
#include <cerrno>
#include <fstream>
#include <string>
#include <typeinfo>


namespace d2 {

/**
 * Base class for exceptions related to the `Repository` class.
 */
struct RepositoryException : virtual Exception {
    virtual char const* what() const throw() {
        return "d2::RepositoryException";
    }
};

/**
 * Exception thrown when a `Repository` is created with an invalid path.
 */
struct InvalidRepositoryPathException : virtual RepositoryException {
    virtual char const* what() const throw() {
        return "d2::InvalidRepositoryPathException";
    }
};

/**
 * Exception thrown when a `Repository` is unable to open a new stream.
 */
struct StreamApertureException : virtual RepositoryException {
    virtual char const* what() const throw() {
        return "d2::StreamApertureException";
    }
};

/**
 * Default mapping policy using `boost::unordered_map`s to map keys to values.
 */
struct boost_unordered_map {
    template <typename Key, typename Value>
    struct apply {
        typedef boost::unordered_map<Key, Value> type;
    };
};

/**
 * Mapping policy using no map at all. All instances of the same key type are
 * mapped to the same value.
 *
 * @note Several methods of `Repository` can't be used with this pseudo-map.
 *       Specifically, `items`, `values` and `keys` won't work because
 *       this pseudo-map can't behave like a range.
 * @todo Have a locking policy for a lock during the whole
 *       `fetch_stream_and_do` operation. This would allow the user to use
 *       only 1 lock to synchronize the whole operation.
 */
struct unary_map {
    template <typename Key, typename Value>
    struct apply {
        struct type {
            typedef Value mapped_type;
            typedef Key key_type;

            mapped_type& operator[](key_type const&) {
                return value_ ? *value_ : *(value_ = boost::in_place());
            }

            bool empty() const {
                return !value_;
            }

        private:
            boost::optional<mapped_type> value_;
        };
    };
};

/**
 * Default locking policy providing no synchronization at all.
 */
struct no_synchronization {
    template <typename Category, typename Stream>
    struct apply {
        struct type {
            void lock() { }
            void unlock() { }
        };
    };
};

/**
 * Locking policy providing synchronization by using some provided
 * synchronization object.
 */
template <typename Mutex>
struct synchronize_with {
    template <typename Category, typename Stream>
    struct apply {
        struct type {
            void lock() { mutex_.lock(); }
            void unlock() { mutex_.unlock(); }

        private:
            Mutex mutex_;
        };
    };
};

/**
 * Class representing a repository into which stuff can be stored.
 */
template <typename Categories,
          typename MappingPolicy = boost_unordered_map,
          typename CategoryLockingPolicy = no_synchronization,
          typename StreamLockingPolicy = no_synchronization>
class Repository {

    template <typename Category>
    struct Bundle {
        // The category to which this bundle is associated.
        typedef Category category_type;

        // The actual type of the streams owned by this bundle.
        // Note: A policy for choosing input only, output only
        //       or input/output would be nice.
        typedef std::fstream stream_type;

        // The object synchronizing accesses to this bundle.
        typedef typename boost::mpl::apply<
                            CategoryLockingPolicy, category_type, stream_type
                        >::type category_locker_type;

        // The object synchronizing accesses to every stream of this bundle.
        // Note: Each stream has a copy of the stream_locker_type to achieve
        //       possibly _very_ granular locking.
        typedef typename boost::mpl::apply<
                            StreamLockingPolicy, category_type, stream_type
                        >::type stream_locker_type;

        // The type of the objects stored in the associative container.
        struct mapped_type {
            stream_locker_type stream_locker;
            stream_type stream;

            typedef Bundle bundle_type;
        };

        // The associative container associated to this bundle.
        typedef typename boost::mpl::apply<
                            MappingPolicy, category_type, mapped_type
                        >::type map_type;

        map_type map;
        category_locker_type category_locker;
    };

    // Associate a Category to its Bundle into a fusion pair.
    struct create_category_bundle {
        template <typename Category>
        struct apply {
            typedef boost::fusion::pair<Category, Bundle<Category> > type;
        };
    };

    // Create a sequence of (Category, Bundle) pairs from which we'll be able
    // to create the fusion map below.
    typedef typename boost::mpl::transform<
                Categories, create_category_bundle
            >::type Zipped;

    // Create a compile-time map from categories (types used as a tag) to
    // their associated bundle.
    typedef typename boost::fusion::result_of::as_map<Zipped>::type BundleMap;

    // Note: Don't try to use this map. Use the accessor below instead!
    BundleMap bundle_map_;

    // Return the bundle associated to a Category in the compile-time map.
    template <typename Category>
    struct bundle_of {
        typedef typename boost::remove_reference<
                    typename boost::fusion::result_of::at_key<
                        BundleMap, Category
                    >::type
                >::type type;

        typedef typename boost::remove_reference<
                    typename boost::fusion::result_of::at_key<
                        BundleMap const, Category
                    >::type
                >::type const_type;

        template <typename Repo>
        type& operator()(Repo& repo) const {
            return boost::fusion::at_key<Category>(repo.bundle_map_);
        }

        template <typename Repo>
        const_type& operator()(Repo const& repo) const {
            return boost::fusion::at_key<Category>(repo.bundle_map_);
        }
    };

    // Accessor for the .stream member that is not yet fully bound.
    template <typename MappedType>
    struct stream_accessor_helper
        : sandbox::member_accessor<
            typename MappedType::bundle_type::stream_type MappedType::*,
            &MappedType::stream
        >
    { };

    // Accessor for the .stream member.
    template <typename NextAccessor = sandbox::identity_accessor>
    struct stream_accessor
        : sandbox::rebind_accessor<
            stream_accessor_helper, NextAccessor
        >
    { };

    template <typename Category, typename Accessor>
    class make_view {
        typedef typename bundle_of<Category>::type Bundle;
        typedef typename bundle_of<Category>::const_type ConstBundle;

    public:
        typedef sandbox::container_view<
                    typename Bundle::map_type, Accessor
                > type;

        typedef sandbox::container_view<
                    typename ConstBundle::map_type, Accessor
                > const_type;
    };

public:
    template <typename Category>
    struct key_view
        : make_view<Category, sandbox::first_accessor<> >
    { };

    template <typename Category>
    struct value_view
        : make_view<Category, sandbox::second_accessor<stream_accessor<> > >
    { };

    template <typename Category>
    typename value_view<Category>::type values() {
        typedef typename value_view<Category>::type View;
        return View(bundle_of<Category>()(*this).map);
    }

    template <typename Category>
    typename value_view<Category>::const_type values() const {
        typedef typename value_view<Category>::const_type View;
        return View(bundle_of<Category>()(*this).map);
    }

    template <typename Category>
    typename key_view<Category>::type keys() {
        typedef typename key_view<Category>::type View;
        return View(bundle_of<Category>()(*this).map);
    }

    template <typename Category>
    typename key_view<Category>::const_type keys() const {
        typedef typename key_view<Category>::const_type View;
        return View(bundle_of<Category>()(*this).map);
    }

private:
    boost::filesystem::path const root_;

    template <typename Category>
    boost::filesystem::path category_path_for() const {
        boost::filesystem::path path(root_);
        return path /= typeid(Category).name();
    }

    struct open_category {
        Repository* this_;

        explicit open_category(Repository* this_) : this_(this_) { }

        typedef void result_type;

        template <typename CategoryBundlePair>
        result_type operator()(CategoryBundlePair const&) const {
            namespace fs = boost::filesystem;
            typedef typename CategoryBundlePair::first_type Category;
            typedef typename bundle_of<Category>::type Bundle;
            typedef typename Bundle::map_type AssociativeContainer;

            Bundle& bundle = bundle_of<Category>()(*this_);
            AssociativeContainer& streams = bundle.map;
            BOOST_ASSERT_MSG(streams.empty(),
                "Opening a category that already has some open streams.");

            fs::path path = this_->category_path_for<Category>();
            BOOST_ASSERT_MSG(!fs::exists(path) || fs::is_directory(path),
                "What should be a category directory is not a directory. "
                "Since we're in charge inside the repository, this is a "
                "programming error.");

            // We create a new directory if it does not already exist.
            // If it did exist, we open all the streams that reside inside
            // that category. Otherwise, there are no streams to open inside
            // the category directory (we just created it), so we're done.
            if (!fs::create_directory(path)) {
                fs::directory_iterator first(path), last;
                for (; first != last; ++first) {
                    fs::path file(*first);
                    BOOST_ASSERT_MSG(fs::is_regular_file(file),
                        "For the moment, there should not be anything else "
                        "than regular files inside a category directory.");
                    // Here, we translate a file name that was generated
                    // using a category instance back to it.
                    Category const& category = boost::lexical_cast<Category>(
                                                    file.filename().string());
                    BOOST_ASSERT_MSG(!streams[category].stream.is_open(),
                        "While opening a category, opening a stream that "
                        "we already know of.");
                    this_->open_stream(streams[category].stream, category);
                }
            }

        }
    };

public:
    /**
     * Create a repository at the path described by `root`.
     * The path must either point to nothing or to an existing directory.
     *
     * If the path points to an existing directory, the directory is used
     * as-if it was previously a repository. Otherwise, a new repository
     * is created.
     *
     * If the path is of any other nature, an exception is thrown.
     *
     * @note `Source` may be any type compatible with
     *       `boost::filesystem::path`'s constructor.
     */
    template <typename Source>
    explicit Repository(Source const& root) : root_(root) {
        namespace fs = boost::filesystem;
        if (fs::exists(root_) && !fs::is_directory(root_))
            D2_THROW(InvalidRepositoryPathException()
                        << boost::errinfo_file_name(root_.c_str()));
        fs::create_directories(root_);

        boost::fusion::for_each(bundle_map_, open_category(this));
    }

private:
    template <typename Category>
    boost::filesystem::path path_for(Category const& category) const {
        return category_path_for<Category>() /=
                                boost::lexical_cast<std::string>(category);
    }

    /**
     * Open a stream in the given category for the first time.
     *
     * @warning This method does not synchronize its access to `stream`.
     *          It is the caller's responsibility to make sure `stream`
     *          can be modified safely.
     */
    template <typename Category>
    void open_stream(typename bundle_of<Category>::type::stream_type& stream,
                     Category const& category) const {
        namespace fs = boost::filesystem;
        BOOST_ASSERT_MSG(!stream.is_open(),
            "Opening a stream that is already open.");

        fs::path path = path_for(category);
        if (fs::exists(path) && !fs::is_regular_file(path))
            D2_THROW(StreamApertureException()
                        << boost::errinfo_file_name(path.c_str()));

        // Try opening an existing file with the same name.
        stream.open(path.c_str(), std::ios_base::in | std::ios_base::out);
        if (!stream.is_open())
            // Create a new, empty file otherwise.
            stream.open(path.c_str(), std::ios_base::in |
                                      std::ios_base::out |
                                      std::ios_base::trunc);
        if (!stream)
            D2_THROW(StreamApertureException()
                        << boost::errinfo_errno(errno)
                        << boost::errinfo_file_name(path.c_str()));
    }

    /**
     * Simple helper class that will call the `lock` method of an object
     * on construction and call its `unlock` method on destruction.
     *
     * This _very_ important to make sure the locks are released if an
     * exception is thrown.
     */
    template <typename Lock>
    struct ScopedLock {
        Lock& lock_;

        explicit ScopedLock(Lock& lock) : lock_(lock) {
            lock_.lock();
        }

        ~ScopedLock() {
            lock_.unlock();
        }
    };

    /**
     * Fetch a stream into its category, perform some action on it and then
     * return a reference to it. Accesses to shared structures is synchronized
     * using the different locking policies. See below for details.
     */
    template <typename Category, typename F>
    typename bundle_of<Category>::type::stream_type&
    fetch_stream_and_do(Category const& category, F const& f) {
        typedef typename bundle_of<Category>::type Bundle;
        typedef typename Bundle::category_locker_type CategoryLocker;
        typedef typename Bundle::map_type AssociativeContainer;

        typedef typename Bundle::mapped_type StreamBundle;
        typedef typename Bundle::stream_locker_type StreamLocker;
        typedef typename Bundle::stream_type Stream;

        Bundle& bundle = bundle_of<Category>()(*this);
        CategoryLocker& category_locker = bundle.category_locker;
        AssociativeContainer& streams = bundle.map;

        // Use the locker to synchronize the map lookup at the category
        // level. The whole category is locked, so it is not possible for
        // another thread to access the associative map at the same time.
        // Note: The usage of a pointer here is required because we can't
        //       initialize the reference inside the scope of the scoped lock.
        StreamBundle* stream_bundle_ptr;
        {
            ScopedLock<CategoryLocker> lock(category_locker);
            stream_bundle_ptr = &streams[category];
        }
        StreamLocker& stream_locker = stream_bundle_ptr->stream_locker;
        Stream& stream = stream_bundle_ptr->stream;

        // Use the locker to synchronize the aperture of the stream at the
        // stream level. Only this stream is locked, so it is not possible for
        // another thread to access this stream at the same time, but it is
        // perfectly possible (and okay) if other threads access other streams
        // in the same category (or in other categories).
        {
            ScopedLock<StreamLocker> lock(stream_locker);
            if (!stream.is_open())
                open_stream(stream, category);
            // Perform some action on the stream while it's synchronized.
            f(stream);
        }

        // Any usage of the stream beyond this point must be synchronized by
        // the caller as needed.
        return stream;
    }

public:
    /**
     * Fetch the stream associated to `category` and execute `f` on it,
     * synchronizing optimally access to the stream.
     *
     * @note When `f` is called, the stream on which it is called
     *       (and only that) is locked from other threads. The other streams
     *       in the repository are _NOT_ locked.
     * @note If `f` throws, the stream will be unlocked.
     */
    template <typename Category, typename UnaryFunction>
    void perform(Category const& category, UnaryFunction const& f) {
        fetch_stream_and_do(category, f);
    }

    /**
     * Return the stream associated to an instance of a category.
     *
     * @warning Any access to the returned stream must be synchronized by
     *          the caller as needed.
     */
    template <typename Category>
    typename bundle_of<Category>::type::stream_type&
    operator[](Category const& category) {
        return fetch_stream_and_do(category, boost::phoenix::nothing);
    }

    /**
     * Write `data` to the output stream associated to `category`.
     * This is equivalent to `repository[category] << data`, except
     * the output operation is synchronized internally in an optimal way.
     */
    template <typename Category, typename Data>
    void write(Category const& category, Data const& data) {
        perform(category, boost::phoenix::arg_names::arg1 << data);
    }

    /**
     * Read into `data` from the input stream associated to `category`.
     * This is equivalent to `repository[category] >> data`, except the
     * input operation is synchronized internally in an optimal way.
     */
    template <typename Category, typename Data>
    void read(Category const& category, Data& data) {
        perform(category,
                boost::phoenix::arg_names::arg1 >> boost::phoenix::ref(data));
    }

private:
    struct category_is_empty {
        Repository const* this_;

        explicit category_is_empty(Repository const* this_) : this_(this_) { }

        typedef bool result_type;

        template <typename CategoryBundlePair>
        result_type operator()(CategoryBundlePair const&) const {
            typedef typename CategoryBundlePair::first_type Category;
            return bundle_of<Category>()(*this_).map.empty();
        }
    };

public:
    /**
     * Return whether there are any open open streams in any category of
     * the repository.
     *
     * @warning Synchronization is the responsibility of the caller.
     */
    bool empty() const {
        return boost::fusion::all(bundle_map_, category_is_empty(this));
    }
};

} // end namespace d2

#endif // !D2_REPOSITORY_HPP