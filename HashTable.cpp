
#include <iostream>
#include <vector>
#include <list>
#include <string>
#include <cstdint>
#include <iomanip>

using namespace std;

// Educational password hash function
uint64_t demoHash(const string& s) {
    uint64_t h = 1469598103934665603ULL;

    for (unsigned char c : s) {
        h = (h ^ c) * 1099511628211ULL;
    }

    return h;
}

const string PEPPER = "educational-pepper-not-secret";

// Stores each user's information
struct User {
    string username;
    string salt;
    uint64_t passwordHash = 0;
};

// Creates a user with a salted and peppered password hash
User makeUser(const string& name,
              const string& password,
              const string& salt) {

    User u;
    u.username = name;
    u.salt = salt;
    u.passwordHash =
        demoHash(salt + password + PEPPER);

    return u;
}

// Four collision-handling strategies
enum class Strategy {
    Chaining,
    Linear,
    Quadratic,
    DoubleHash
};

string strategyName(Strategy s) {
    switch (s) {
        case Strategy::Chaining:
            return "Separate chaining";

        case Strategy::Linear:
            return "Linear probing";

        case Strategy::Quadratic:
            return "Quadratic probing";

        default:
            return "Double hashing";
    }
}

// Hash table implementation
class HashTable {

    struct Slot {
        User user;
        bool occupied = false;
        bool deleted = false;
    };

    Strategy strategy;

    vector<list<User>> chains;
    vector<Slot> slots;

    size_t count = 0;
    size_t tombstones = 0;
    size_t rehashEvents = 0;

    static constexpr double MAX_LOAD = 0.50;

    // Returns the current capacity
    size_t capacity() const {
        if (strategy == Strategy::Chaining)
            return chains.size();

        return slots.size();
    }

    // Checks whether a number is prime
    static bool isPrime(size_t n) {
        if (n < 2)
            return false;

        for (size_t i = 2; i <= n / i; ++i) {
            if (n % i == 0)
                return false;
        }

        return true;
    }

    // Finds the next prime number
    static size_t nextPrime(size_t n) {
        while (!isPrime(n)) {
            ++n;
        }

        return n;
    }

    // Calculates new table capacity
    size_t nextCapacity(size_t old) const {
        if (strategy == Strategy::Quadratic)
            return old * 2;

        return nextPrime(old * 2 + 1);
    }

    // Determines the index for each probing method
    size_t position(const string& key,
                    size_t attempt) const {

        size_t m = capacity();
        size_t base = demoHash(key) % m;

        // Linear probing
        if (strategy == Strategy::Linear) {
            return (base + attempt) % m;
        }

        // Quadratic probing using triangular offsets
        if (strategy == Strategy::Quadratic) {
            return (base +
                    attempt * (attempt + 1) / 2) % m;
        }

        // Double hashing
        size_t step =
            1 + demoHash("step:" + key) % (m - 1);

        return (base + attempt * step) % m;
    }

    // Inserts a user without resizing
    bool placeWithoutResize(const User& user) {

        // Separate chaining
        if (strategy == Strategy::Chaining) {

            size_t index =
                demoHash(user.username) % capacity();

            chains[index].push_back(user);

            ++count;
            return true;
        }

        // Open addressing methods
        for (size_t i = 0; i < capacity(); ++i) {

            Slot& slot =
                slots[position(user.username, i)];

            if (!slot.occupied) {

                if (slot.deleted) {
                    --tombstones;
                }

                // Fixed assignment
                slot.user = user;
                slot.occupied = true;
                slot.deleted = false;

                ++count;
                return true;
            }
        }

        // No exceptions needed
        cerr << "Error: hash table is full." << endl;
        return false;
    }

    // Automatically resizes and rehashes users
    void rehash(size_t newCapacity) {

        size_t oldCapacity = capacity();
        vector<User> oldUsers;

        if (strategy == Strategy::Chaining) {

            for (const auto& chain : chains) {
                for (const auto& user : chain) {
                    oldUsers.push_back(user);
                }
            }

            chains.assign(newCapacity, list<User>());

        } else {

            for (const auto& slot : slots) {
                if (slot.occupied) {
                    oldUsers.push_back(slot.user);
                }
            }

            slots.assign(newCapacity, Slot());
        }

        count = 0;
        tombstones = 0;

        for (const auto& user : oldUsers) {
            placeWithoutResize(user);
        }

        ++rehashEvents;

        cout << "REHASH: "
             << oldCapacity
             << " -> "
             << newCapacity
             << endl;
    }

public:

    // Constructor
    explicit HashTable(Strategy s) : strategy(s) {

        if (s == Strategy::Quadratic) {
            slots.resize(8);
        }
        else if (s == Strategy::Chaining) {
            chains.resize(5);
        }
        else {
            slots.resize(5);
        }
    }

    // Returns number of users
    size_t size() const {
        return count;
    }

    // Returns number of buckets
    size_t bucketCount() const {
        return capacity();
    }

    // Returns number of rehash events
    size_t resizeCount() const {
        return rehashEvents;
    }

    // Calculates load factor
    double loadFactor() const {
        return static_cast<double>(count) / capacity();
    }

    // Searches for a user
    const User* find(const string& username) const {

        if (strategy == Strategy::Chaining) {

            size_t index =
                demoHash(username) % capacity();

            for (const auto& user : chains[index]) {

                if (user.username == username) {
                    return &user;
                }
            }

            return nullptr;
        }

        for (size_t i = 0; i < capacity(); ++i) {

            const Slot& slot =
                slots[position(username, i)];

            if (!slot.occupied && !slot.deleted) {
                return nullptr;
            }

            if (slot.occupied &&
                slot.user.username == username) {

                return &slot.user;
            }
        }

        return nullptr;
    }

    // Inserts a new user
    bool insert(const User& user) {

        // Prevent duplicate usernames
        if (find(user.username)) {
            return false;
        }

        double nextLoad =
            static_cast<double>(count + 1) / capacity();

        // Resize when load factor becomes too high
        if (nextLoad > MAX_LOAD) {

            rehash(nextCapacity(capacity()));

        }
        else if (strategy != Strategy::Chaining &&
                 tombstones > capacity() / 4) {

            // Clean up deleted slots
            rehash(capacity());
        }

        return placeWithoutResize(user);
    }

    // Removes an existing user
    bool remove(const string& username) {

        if (strategy == Strategy::Chaining) {

            size_t index =
                demoHash(username) % capacity();

            auto& chain = chains[index];

            for (auto it = chain.begin();
                 it != chain.end(); ++it) {

                if (it->username == username) {

                    chain.erase(it);
                    --count;

                    return true;
                }
            }

            return false;
        }

        // Removal for probing methods
        for (size_t i = 0; i < capacity(); ++i) {

            Slot& slot =
                slots[position(username, i)];

            if (!slot.occupied && !slot.deleted) {
                return false;
            }

            if (slot.occupied &&
                slot.user.username == username) {

                slot.occupied = false;
                slot.deleted = true;

                --count;
                ++tombstones;

                return true;
            }
        }

        return false;
    }

    // Verifies username and password
    bool login(const string& username,
               const string& password) const {

        const User* user = find(username);

        if (!user) {
            return false;
        }

        uint64_t submittedHash =
            demoHash(user->salt + password + PEPPER);

        return submittedHash == user->passwordHash;
    }
};

// Main program and testing
int main() {

    // Six users with unique salts and passwords
    vector<User> users = {

        makeUser("alex", "Maple!42", "salt-a1"),

        makeUser("blair", "Ocean!55", "salt-b2"),

        makeUser("casey", "Cloud!68", "salt-c3"),

        makeUser("drew", "River!71", "salt-d4"),

        makeUser("ellis", "Stone!83", "salt-e5"),

        makeUser("frankie", "Amber!96", "salt-f6")
    };

    cout << boolalpha
         << fixed
         << setprecision(2);

    // Test all four collision-handling strategies
    for (Strategy s : {

        Strategy::Chaining,
        Strategy::Linear,
        Strategy::Quadratic,
        Strategy::DoubleHash

    }) {

        cout << "\n== "
             << strategyName(s)
             << " ==\n";

        HashTable table(s);

        // Insert all six users
        for (const auto& user : users) {
            table.insert(user);
        }

        // Show table statistics
        cout << "Users: "
             << table.size()
             << ", Capacity: "
             << table.bucketCount()
             << ", Load factor: "
             << table.loadFactor()
             << endl;

        // Test successful login
        cout << "Correct password: "
             << table.login("alex", "Maple!42")
             << endl;

        // Test failed password
        cout << "Wrong password: "
             << table.login("alex", "wrong")
             << endl;

        // Test unknown username
        cout << "Unknown user: "
             << table.login("nobody", "anything")
             << endl;

        // Test duplicate username rejection
        cout << "Duplicate rejected: "
             << !table.insert(users[0])
             << endl;

        // Remove a user
        cout << "Remove blair: "
             << table.remove("blair")
             << endl;

        // Verify removed account cannot log in
        cout << "Removed user login fails: "
             << !table.login("blair", "Ocean!55")
             << endl;

        // Verify another account still works
        cout << "Remaining user works: "
             << table.login("casey", "Cloud!68")
             << endl;

        // Add removed account back with new password
        cout << "Reinsert with new password: "
             << table.insert(
                 makeUser("blair",
                          "NewPass!1",
                          "new-salt"))
             << endl;

        // Verify old password no longer works
        cout << "Old password rejected: "
             << !table.login("blair", "Ocean!55")
             << endl;

        // Verify new password works
        cout << "New password works: "
             << table.login("blair", "NewPass!1")
             << endl;

        // Display rehash count
        cout << "Rehash events: "
             << table.resizeCount()
             << endl;
    }

    return 0;
}
