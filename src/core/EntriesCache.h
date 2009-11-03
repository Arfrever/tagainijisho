/*
 *  Copyright (C) 2008  Alexandre Courbot
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __CORE_ENTRIES_CACHE_H
#define __CORE_ENTRIES_CACHE_H

#include "core/Preferences.h"
#include "core/Entry.h"

#include <QHash>
#include <QPair>
#include <QObject>
#include <QQueue>
#include <QMutex>

/**
 * A reference-counting entry pointer. It is designed to only
 * accept members of Entry type and properly deletes them when
 * their reference count reaches zero. It is largely inspired by
 * Qt's QExplicitlySharedDataPointer class, with the difference
 * that is removes the entry from the list of loaded entries in
 * the EntriesCache before actually deleting it, ensuring atomic
 * deletion of entries.
 */
template <class T> class EntryPointer
{
private:
	void _ref() const { if (d) d->ref.ref(); }
	void _deref();
public:
    typedef T Type;

    inline T &operator*() { return *d; }
    inline const T &operator*() const { return *d; }
    inline T *operator->() { return d; }
    inline T *operator->() const { return d; }
    inline T *data() const { return d; }
    inline const T *constData() const { return d; }

    inline void reset()
    {
    	_deref();
        d = 0;
    }

    inline operator bool () const { return d != 0; }

    inline bool operator==(const EntryPointer<T> &other) const { return d == other.d; }
    inline bool operator!=(const EntryPointer<T> &other) const { return d != other.d; }
    inline bool operator==(const T *ptr) const { return d == ptr; }
    inline bool operator!=(const T *ptr) const { return d != ptr; }

    inline EntryPointer() { d = 0; }
    inline ~EntryPointer() { _deref(); }

    explicit EntryPointer(T *data) : d(data) { _ref(); }
    inline EntryPointer(const EntryPointer<T> &o) : d(o.d) { _ref(); }

#ifndef QT_NO_MEMBER_TEMPLATES
    template<class X>
    inline EntryPointer(const EntryPointer<X> &o) : d(static_cast<T *>(o.data())) { _ref(); }
#endif

    inline EntryPointer<T> & operator=(const EntryPointer<T> &o) {
        if (o.d != d) {
        	o._ref();
        	_deref();
            d = o.d;
        }
        return *this;
    }
    inline EntryPointer &operator=(T *o) {
        if (o != d) {
            if (o)
                o->ref.ref();
            _deref();
            d = o;
        }
        return *this;
    }

    inline bool operator!() const { return !d; }

private:
    T *d;
};

/**
 * The EntryCache plays a double role:
 *
 * # It ensures that there is no more than a unique instance of every Entry
 *   loaded at the same time, for consistency and memory saving reasons.
 * # It keeps frequently used entries in memory to avoid reloading them
 *   from the database every time they are necessary and increase
 *   performances.
 *
 * ALL Entry loading operations MUST be performed through this class, which
 * is the only one allowed to access the Entry loaders. Respecting this rule
 * ensures data consistency and safety, and greatly simplify the UI design.
 */
class EntriesCache : public QObject
{
	Q_OBJECT
private:
	static EntriesCache * _instance;

	QHash<QPair<int, int>, Entry *> _loadedEntries;
	QMutex _loadedEntriesMutex;

	QQueue<EntryPointer<Entry> > _cache;
	QMutex _cacheMutex;

	/**
	 * Removes the given entry from the entries list, then delete it.
	 * This operation is atomic, i.e. it ensures no reference to the
	 * deleted entries are created by the meantime.
	 */
	void _removeAndDelete(const Entry *entry);

	friend class Entry;

	EntryPointer<Entry> _get(int type, int id);
	EntriesCache(QObject *parent = 0);
	~EntriesCache();

friend class EntryPointer<Entry>;
friend class EntryPointer<const Entry>;

public:
	static void init();
	static void cleanup();

	/**
	 * Returns the unique instance of the entry given as argument,
	 * loading it from the database if necessary. Returns null
	 * if the entry could not be loaded.
	 */
	static EntryPointer<Entry> get(int type, int id) { return _instance->_get(type, id); }

	/**
	 * The size of the cache can be modified in real-time through this value.
	 */
	static PreferenceItem<int> cacheSize;
};

template <class T> void EntryPointer<T>::_deref()
{
	if(d && !d->ref.deref()) {
		EntriesCache::_instance->_removeAndDelete(d);
	}
}

#endif
