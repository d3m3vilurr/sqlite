
#ifndef __SQLITESESSION_H_
#define __SQLITESESSION_H_ 1

/*
** Make sure we can call this stuff from C++.
*/
#ifdef __cplusplus
extern "C" {
#endif

#include "sqlite3.h"

/*
** CAPI3REF: Session Object Handle
*/
typedef struct sqlite3_session sqlite3_session;

/*
** CAPI3REF: Changeset Iterator Handle
*/
typedef struct sqlite3_changeset_iter sqlite3_changeset_iter;

/*
** CAPI3REF: Create A New Session Object
**
** Create a new session object attached to database handle db. If successful,
** a pointer to the new object is written to *ppSession and SQLITE_OK is
** returned. If an error occurs, *ppSession is set to NULL and an SQLite
** error code (e.g. SQLITE_NOMEM) is returned.
**
** It is possible to create multiple session objects attached to a single
** database handle.
**
** Session objects created using this function should be deleted using the
** [sqlite3session_delete()] function before the database handle that they
** are attached to is itself closed. If the database handle is closed before
** the session object is deleted, then the results of calling any session
** module function, including [sqlite3session_delete()] on the session object
** are undefined.
**
** Because the session module uses the [sqlite3_preupdate_hook()] API, it
** is not possible for an application to register a pre-update hook on a
** database handle that has one or more session objects attached. Nor is
** it possible to create a session object attached to a database handle for
** which a pre-update hook is already defined. The results of attempting 
** either of these things are undefined.
**
** The session object will be used to create changesets for tables in
** database zDb, where zDb is either "main", or "temp", or the name of an
** attached database. It is not an error if database zDb does not exist
** to the database when the session object is created.
*/
int sqlite3session_create(
  sqlite3 *db,                    /* Database handle */
  const char *zDb,                /* Name of db (e.g. "main") */
  sqlite3_session **ppSession     /* OUT: New session object */
);

/*
** CAPI3REF: Delete A Session Object
**
** Delete a session object previously allocated using 
** [sqlite3session_create()]. Once a session object has been deleted, the
** results of attempting to use pSession with any other session module
** function are undefined.
**
** Session objects must be deleted before the database handle to which they
** are attached is closed. Refer to the documentation for 
** [sqlite3session_create()] for details.
*/
void sqlite3session_delete(sqlite3_session *pSession);

/*
** CAPI3REF: Enable Or Disable A Session Object
**
** Enable or disable the recording of changes by a session object. When
** enabled, a session object records changes made to the database. When
** disabled - it does not. A newly created session object is enabled.
** Refer to the documentation for [sqlite3session_changeset()] for further
** details regarding how enabling and disabling a session object affects
** the eventual changesets.
**
** Passing zero to this function disables the session. Passing a value
** greater than zero enables it. Passing a value less than zero is a 
** no-op, and may be used to query the current state of the session.
**
** The return value indicates the final state of the session object: 0 if 
** the session is disabled, or 1 if it is enabled.
*/
int sqlite3session_enable(sqlite3_session *pSession, int bEnable);

/*
** CAPI3REF: Attach a Table to a Session Object
**
** Attach a table to a session. All subsequent changes made to the table
** while the session object is enabled will be recorded. See documentation
** for [sqlite3session_changeset()] for further details.
**
** Changes can only be recorded for tables that have a PRIMARY KEY explicitly
** defined as part of their CREATE TABLE statement. It does not matter if the 
** PRIMARY KEY is an "INTEGER PRIMARY KEY" (rowid alias) or not. The PRIMARY
** KEY may consist of a single column, or may be a composite key.
** 
** It is not an error if the named table does not exist in the database. Nor
** is it an error if the named table does not have a PRIMARY KEY. However,
** no changes will be recorded in either of these scenarios.
**
** SQLITE_OK is returned if the table is successfully attached to the session
** object. Or, if an error occurs, an SQLite error code (e.g. SQLITE_NOMEM)
** is returned.
*/
int sqlite3session_attach(
  sqlite3_session *pSession,      /* Session object */
  const char *zTab                /* Table name */
);

/*
** CAPI3REF: Generate A Changeset From A Session Object
**
** Obtain a changeset containing changes to the tables attached to the 
** session object passed as the first argument. If successful, 
** set *ppChangeset to point to a buffer containing the changeset 
** and *pnChangeset to the size of the changeset in bytes before returning
** SQLITE_OK. If an error occurs, set both *ppChangeset and *pnChangeset to
** zero and return an SQLite error code.
**
** A changeset consists of zero or more INSERT, UPDATE and/or DELETE changes,
** each representing a change to a single row of an attached table. An INSERT
** change contains the values of each field of a new database row. A DELETE
** contains the original values of each field of a deleted database row. An
** UPDATE change contains the original values of each field of an updated
** database row along with the updated values for each updated non-primary-key
** column. It is not possible for an UPDATE change to represent a change that
** modifies the values of primary key columns. If such a change is made, it
** is represented in a changeset as a DELETE followed by an INSERT.
**
** The contents of a changeset may be traversed using an iterator created
** using the [sqlite3changeset_start()] API. A changeset may be applied to
** a database with a compatible schema using the [sqlite3changeset_apply()]
** API.
**
** Following a successful call to this function, it is the responsibility of
** the caller to eventually free the buffer that *ppChangeset points to using
** [sqlite3_free()].
**
** <h3>Changeset Generation</h3>
**
** Once a table has been attached to a session object, the session object
** records the primary key values of all new rows inserted into the table.
** It also records the original primary key and other column values of any
** deleted or updated rows. For each unique primary key value, data is only
** recorded once - the first time a row with said primary key is inserted,
** updated or deleted in the lifetime of the session.
**
** The session object therefore accumulates two types of records - those
** that consist of primary key values only (created when the user inserts
** a new record) and those that consist of the primary key values and the
** original values of other table columns (created when the users deletes
** or updates a record).
**
** When this function is called, the requested changeset is created using
** both the accumulated records and the current contents of the database
** file. Specifically:
**
** <ul>
**   <li> For each record generated by an insert, the database is queried
**        for a row with a matching primary key. If one is found, an INSERT
**        change is added to the changeset. If no such row is found, no change 
**        is added to the changeset.
**
**   <li> For each record generated by an update or delete, the database is 
**        queried for a row with a matching primary key. If such a row is
**        found and one or more of the non-primary key fields have been
**        modified from their original values, an UPDATE change is added to 
**        the changeset. Or, if no such row is found in the table, a DELETE 
**        change is added to the changeset. If there is a row with a matching
**        primary key in the database, but all fields contain their original
**        values, no change is added to the changeset.
** </ul>
**
** This means, amongst other things, that if a row is inserted and then later
** deleted while a session object is active, neither the insert or the delete
** will be present in the changeset. Or if a row is deleted and then later a 
** row with the same primary key values inserted while a session object is
** active, the resulting changeset will contain an UPDATE change instead of
** a DELETE and an INSERT.
**
** When a session object is disabled (see the [sqlite3session_enable()] API),
** it does not accumulate records when rows are inserted, updated or deleted.
** This may appear to have some counter-intuitive effects if a single row
** is written to more than once during a session. For example, if a row
** is inserted while a session object is enabled, then later deleted while 
** the same session object is disabled, no INSERT record will appear in the
** changeset, even though the delete took place while the session was disabled.
** Or, if one field of a row is updated while a session is disabled, and 
** another field of the same row is updated while the session is enabled, the
** resulting changeset will contain an UPDATE change that updates both fields.
*/
int sqlite3session_changeset(
  sqlite3_session *pSession,      /* Session object */
  int *pnChangeset,               /* OUT: Size of buffer at *ppChangeset */
  void **ppChangeset              /* OUT: Buffer containing changeset */
);

/*
** CAPI3REF: Create An Iterator To Traverse A Changeset 
**
** Create an iterator used to iterate through the contents of a changeset.
** If successful, *pp is set to point to the iterator handle and SQLITE_OK
** is returned. Otherwise, if an error occurs, *pp is set to zero and an
** SQLite error code is returned.
**
** The following functions can be used to advance and query a changeset 
** iterator created by this function:
**
** <ul>
**   <li> [sqlite3changeset_next()]
**   <li> [sqlite3changeset_op()]
**   <li> [sqlite3changeset_new()]
**   <li> [sqlite3changeset_old()]
** </ul>
**
** It is the responsibility of the caller to eventually destroy the iterator
** by passing it to [sqlite3changeset_finalize()]. The buffer containing the
** changeset (pChangeset) must remain valid until after the iterator is
** destroyed.
*/
int sqlite3changeset_start(
  sqlite3_changeset_iter **pp,    /* OUT: New changeset iterator handle */
  int nChangeset,                 /* Size of changeset blob in bytes */
  void *pChangeset                /* Pointer to blob containing changeset */
);

/*
** CAPI3REF: Advance A Changeset Iterator
**
** This function may only be used with iterators created by function
** [sqlite3changeset_start()]. If it is called on an iterator passed to
** a conflict-handler callback by [sqlite3changeset_apply()], SQLITE_MISUSE
** is returned and the call has no effect.
**
** Immediately after an iterator is created by sqlite3changeset_start(), it
** does not point to any change in the changeset. Assuming the changeset
** is not empty, the first call to this function advances the iterator to
** point to the first change in the changeset. Each subsequent call advances
** the iterator to point to the next change in the changeset (if any). If
** no error occurs and the iterator points to a valid change after a call
** to sqlite3changeset_next() has advanced it, SQLITE_ROW is returned. 
** Otherwise, if all changes in the changeset have already been visited,
** SQLITE_DONE is returned.
**
** If an error occurs, an SQLite error code is returned. Possible error 
** codes include SQLITE_CORRUPT (if the changeset buffer is corrupt) or 
** SQLITE_NOMEM.
*/
int sqlite3changeset_next(sqlite3_changeset_iter *pIter);

/*
** CAPI3REF: Obtain The Current Operation From A Changeset Iterator
**
** The pIter argument passed to this function may either be an iterator
** passed to a conflict-handler by [sqlite3changeset_apply()], or an iterator
** created by [sqlite3changeset_start()]. In the latter case, the most recent
** call to [sqlite3changeset_next()] must have returned [SQLITE_ROW]. If this
** is not the case, this function returns [SQLITE_MISUSE].
**
** If argument pzTab is not NULL, then *pzTab is set to point to a
** nul-terminated utf-8 encoded string containing the name of the table
** affected by the current change. The buffer remains valid until either
** sqlite3changeset_next() is called on the iterator or until the 
** conflict-handler function returns. If pnCol is not NULL, then *pnCol is 
** set to the number of columns in the table affected by the change. Finally,
** if pOp is not NULL, then *pOp is set to one of [SQLITE_INSERT], 
** [SQLITE_DELETE] or [SQLITE_UPDATE], depending on the type of change that 
** the iterator currently points to.
**
** If no error occurs, SQLITE_OK is returned. If an error does occur, an
** SQLite error code is returned. The values of the output variables may not
** be trusted in this case.
*/
int sqlite3changeset_op(
  sqlite3_changeset_iter *pIter,  /* Iterator object */
  const char **pzTab,             /* OUT: Pointer to table name */
  int *pnCol,                     /* OUT: Number of columns in table */
  int *pOp                        /* OUT: SQLITE_INSERT, DELETE or UPDATE */
);

/*
** CAPI3REF: Obtain old.* Values From A Changeset Iterator
**
** The pIter argument passed to this function may either be an iterator
** passed to a conflict-handler by [sqlite3changeset_apply()], or an iterator
** created by [sqlite3changeset_start()]. In the latter case, the most recent
** call to [sqlite3changeset_next()] must have returned SQLITE_ROW. 
** Furthermore, it may only be called if the type of change that the iterator
** currently points to is either [SQLITE_DELETE] or [SQLITE_UPDATE]. Otherwise,
** this function returns [SQLITE_MISUSE] and sets *ppValue to NULL.
**
** Argument iVal must be greater than or equal to 0, and less than the number
** of columns in the table affected by the current change. Otherwise,
** [SQLITE_RANGE] is returned and *ppValue is set to NULL.
**
** If successful, this function sets *ppValue to point to a protected
** sqlite3_value object containing the iVal'th value from the vector of 
** original row values stored as part of the UPDATE or DELETE change and
** returns SQLITE_OK. The name of the function comes from the fact that this 
** is similar to the "old.*" columns available to update or delete triggers.
**
** If some other error occurs (e.g. an OOM condition), an SQLite error code
** is returned and *ppValue is set to NULL.
*/
int sqlite3changeset_old(
  sqlite3_changeset_iter *pIter,  /* Changeset iterator */
  int iVal,                       /* Column number */
  sqlite3_value **ppValue         /* OUT: Old value (or NULL pointer) */
);

/*
** CAPI3REF: Obtain new.* Values From A Changeset Iterator
**
** The pIter argument passed to this function may either be an iterator
** passed to a conflict-handler by [sqlite3changeset_apply()], or an iterator
** created by [sqlite3changeset_start()]. In the latter case, the most recent
** call to [sqlite3changeset_next()] must have returned SQLITE_ROW. 
** Furthermore, it may only be called if the type of change that the iterator
** currently points to is either [SQLITE_UPDATE] or [SQLITE_INSERT]. Otherwise,
** this function returns [SQLITE_MISUSE] and sets *ppValue to NULL.
**
** Argument iVal must be greater than or equal to 0, and less than the number
** of columns in the table affected by the current change. Otherwise,
** [SQLITE_RANGE] is returned and *ppValue is set to NULL.
**
** If successful, this function sets *ppValue to point to a protected
** sqlite3_value object containing the iVal'th value from the vector of 
** new row values stored as part of the UPDATE or INSERT change and
** returns SQLITE_OK. If the change is an UPDATE and does not include
** a new value for the requested column, *ppValue is set to NULL and 
** SQLITE_OK returned. The name of the function comes from the fact that 
** this is similar to the "new.*" columns available to update or delete 
** triggers.
**
** If some other error occurs (e.g. an OOM condition), an SQLite error code
** is returned and *ppValue is set to NULL.
*/
int sqlite3changeset_new(
  sqlite3_changeset_iter *pIter,  /* Changeset iterator */
  int iVal,                       /* Column number */
  sqlite3_value **ppValue         /* OUT: New value (or NULL pointer) */
);

/*
** CAPI3REF: Obtain Conflicting Row Values From A Changeset Iterator
**
** This function should only be used with iterator objects passed to a
** conflict-handler callback by [sqlite3changeset_apply()] with either
** [SQLITE_CHANGESET_DATA] or [SQLITE_CHANGESET_CONFLICT]. If this function
** is called on any other iterator, [SQLITE_MISUSE] is returned and *ppValue
** is set to NULL.
**
** Argument iVal must be greater than or equal to 0, and less than the number
** of columns in the table affected by the current change. Otherwise,
** [SQLITE_RANGE] is returned and *ppValue is set to NULL.
**
** If successful, this function sets *ppValue to point to a protected
** sqlite3_value object containing the iVal'th value from the 
** "conflicting row" associated with the current conflict-handler callback
** and returns SQLITE_OK.
**
** If some other error occurs (e.g. an OOM condition), an SQLite error code
** is returned and *ppValue is set to NULL.
*/
int sqlite3changeset_conflict(
  sqlite3_changeset_iter *pIter,  /* Changeset iterator */
  int iVal,                       /* Column number */
  sqlite3_value **ppValue         /* OUT: Value from conflicting row */
);


/*
** CAPI3REF: Finalize a Changeset Iterator
**
** This function is used to finalize an iterator allocated with
** [sqlite3changeset_start()].
**
** This function should only be called on iterators created using the
** [sqlite3changeset_start()] function. If an application calls this
** function with an iterator passed to a conflict-handler by
** [sqlite3changeset_apply()], [SQLITE_MISUSE] is immediately returned and the
** call has no effect.
**
** If an error was encountered within a call to an sqlite3changeset_xxx()
** function (for example an [SQLITE_CORRUPT] in [sqlite3changeset_next()] or an 
** [SQLITE_NOMEM] in [sqlite3changeset_new()]) then an error code corresponding
** to that error is returned by this function. Otherwise, SQLITE_OK is
** returned. This is to allow the following pattern (pseudo-code):
**
**   sqlite3changeset_start();
**   while( SQLITE_ROW==sqlite3changeset_next() ){
**     // Do something with change.
**   }
**   rc = sqlite3changeset_finalize();
**   if( rc!=SQLITE_OK ){
**     // An error has occurred 
**   }
*/
int sqlite3changeset_finalize(sqlite3_changeset_iter *pIter);

/*
** CAPI3REF: Invert A Changeset
**
** This function is used to "invert" a changeset object. Applying an inverted
** changeset to a database reverses the effects of applying the uninverted
** changeset. Specifically:
**
** <ul>
**   <li> Each DELETE change is changed to an INSERT, and
**   <li> Each INSERT change is changed to a DELETE, and
**   <li> For each UPDATE change, the old.* and new.* values are exchanged.
** </ul>
**
** If successful, a pointer to a buffer containing the inverted changeset
** is stored in *ppOut, the size of the same buffer is stored in *pnOut, and
** SQLITE_OK is returned. If an error occurs, both *pnOut and *ppOut are
** zeroed and an SQLite error code returned.
**
** It is the responsibility of the caller to eventually call sqlite3_free()
** on the *ppOut pointer to free the buffer allocation following a successful 
** call to this function.
*/
int sqlite3changeset_invert(
  int nIn, void *pIn,             /* Input changeset */
  int *pnOut, void **ppOut        /* OUT: Inverse of input */
);

/*
** CAPI3REF: Apply A Changeset To A Database
**
** Apply a changeset to a database. This function attempts to update the
** "main" database attached to handle db with the changes found in the
** changeset passed via the second and third arguments.
**
** For each change in the changeset, this function tests that the target
** database contains a compatible table. A table is considered compatible
** if all of the following are true:
**
** <ul>
**   <li> The table has the same name as the name recorded in the 
**        changeset, and
**   <li> The table has the same number of columns as recorded in the 
**        changeset, and
**   <li> The table has primary key columns in the same position as 
**        recorded in the changeset.
** </ul>
**
** If there is no compatible table, it is not an error, but the change is
** not applied. A warning message is issued via the sqlite3_log() mechanism
** with the error code SQLITE_SCHEMA. At most one such warning is issued for
** each table in the changeset.
**
** Otherwise, if there is a compatible table, an attempt is made to modify
** the table contents according to the UPDATE, INSERT or DELETE change.
** If a change cannot be applied cleanly, the conflict handler function
** passed as the fourth argument to sqlite3changeset_apply() may be invoked.
** A description of exactly when the conflict handler is invoked for each
** type of change is below.
**
** Each time the conflict handler function is invoked, it must return one
** of [SQLITE_CHANGESET_OMIT], [SQLITE_CHANGESET_ABORT] or 
** [SQLITE_CHANGESET_REPLACE]. SQLITE_CHANGESET_REPLACE may only be returned
** if the second argument passed to the conflict handler is either
** SQLITE_CHANGESET_DATA or SQLITE_CHANGESET_CONFLICT. If the conflict-handler
** returns an illegal value, any changes already made are rolled back and
** the call to sqlite3changeset_apply() returns SQLITE_MISUSE. Different 
** actions are taken by sqlite3changeset_apply() depending on the value
** returned by each invocation of the conflict-handler function. Refer to
** the documentation for the three 
** [SQLITE_CHANGESET_OMIT|available return values] for details.
**
** <dl>
** <dt>DELETE Changes<dd>
**   For each DELETE change, this function checks if the target database 
**   contains a row with the same primary key value (or values) as the 
**   original row values stored in the changeset. If it does, and the values 
**   stored in all non-primary key columns also match the values stored in 
**   the changeset the row is deleted from the target database.
**
**   If a row with matching primary key values is found, but one or more of
**   the non-primary key fields contains a value different from the original
**   row value stored in the changeset, the conflict-handler function is
**   invoked with [SQLITE_CHANGESET_DATA] as the second argument.
**
**   If no row with matching primary key values is found in the database,
**   the conflict-handler function is invoked with [SQLITE_CHANGESET_NOTFOUND]
**   passed as the second argument.
**
**   If the DELETE operation is attempted, but SQLite returns SQLITE_CONSTRAINT
**   (which can only happen if a foreign key constraint is violated), the
**   conflict-handler function is invoked with [SQLITE_CHANGESET_CONSTRAINT]
**   passed as the second argument. This includes the case where the DELETE
**   operation is attempted because an earlier call to the conflict handler
**   function returned [SQLITE_CHANGESET_REPLACE].
**
** <dt>INSERT Changes<dd>
**   For each INSERT change, an attempt is made to insert the new row into
**   the database.
**
**   If the attempt to insert the row fails because the database already 
**   contains a row with the same primary key values, the conflict handler
**   function is invoked with the second argument set to 
**   [SQLITE_CHANGESET_CONFLICT].
**
**   If the attempt to insert the row fails because of some other constraint
**   violation (e.g. NOT NULL or UNIQUE), the conflict handler function is 
**   invoked with the second argument set to [SQLITE_CHANGESET_CONSTRAINT].
**   This includes the case where the INSERT operation is re-attempted because 
**   an earlier call to the conflict handler function returned 
**   [SQLITE_CHANGESET_REPLACE].
**
** <dt>UPDATE Changes<dd>
**   For each UPDATE change, this function checks if the target database 
**   contains a row with the same primary key value (or values) as the 
**   original row values stored in the changeset. If it does, and the values 
**   stored in all non-primary key columns also match the values stored in 
**   the changeset the row is updated within the target database.
**
**   If a row with matching primary key values is found, but one or more of
**   the non-primary key fields contains a value different from an original
**   row value stored in the changeset, the conflict-handler function is
**   invoked with [SQLITE_CHANGESET_DATA] as the second argument. Since
**   UPDATE changes only contain values for non-primary key fields that are
**   to be modified, only those fields need to match the original values to
**   avoid the SQLITE_CHANGESET_DATA conflict-handler callback.
**
**   If no row with matching primary key values is found in the database,
**   the conflict-handler function is invoked with [SQLITE_CHANGESET_NOTFOUND]
**   passed as the second argument.
**
**   If the UPDATE operation is attempted, but SQLite returns 
**   SQLITE_CONSTRAINT, the conflict-handler function is invoked with 
**   [SQLITE_CHANGESET_CONSTRAINT] passed as the second argument.
**   This includes the case where the UPDATE operation is attempted after 
**   an earlier call to the conflict handler function returned
**   [SQLITE_CHANGESET_REPLACE].  
** </dl>
**
** It is safe to execute SQL statements, including those that write to the
** table that the callback related to, from within the xConflict callback.
** This can be used to further customize the applications conflict
** resolution strategy.
**
** All changes made by this function are enclosed in a savepoint transaction.
** If any other error (aside from a constraint failure when attempting to
** write to the target database) occurs, then the savepoint transaction is
** rolled back, restoring the target database to its original state, and an 
** SQLite error code returned.
*/
int sqlite3changeset_apply(
  sqlite3 *db,                    /* Apply change to "main" db of this handle */
  int nChangeset,                 /* Size of changeset in bytes */
  void *pChangeset,               /* Changeset blob */
  int(*xConflict)(
    void *pCtx,                   /* Copy of fifth arg to _apply() */
    int eConflict,                /* DATA, MISSING, CONFLICT, CONSTRAINT */
    sqlite3_changeset_iter *p     /* Handle describing change and conflict */
  ),
  void *pCtx                      /* First argument passed to xConflict */
);

/* 
** CAPI3REF: Constants Passed to The Conflict Handler
**
** Values that may be passed as the second argument to a conflict-handler.
**
** <dl>
** <dt>SQLITE_CHANGESET_DATA<dd>
**   The conflict handler is invoked with CHANGESET_DATA as the second argument
**   when processing a DELETE or UPDATE change if a row with the required
**   PRIMARY KEY fields is present in the database, but one or more other 
**   (non primary-key) fields modified by the update do not contain the 
**   expected "before" values.
** 
**   The conflicting row, in this case, is the database row with the matching
**   primary key.
** 
** <dt>SQLITE_CHANGESET_NOTFOUND<dd>
**   The conflict handler is invoked with CHANGESET_NOTFOUND as the second
**   argument when processing a DELETE or UPDATE change if a row with the
**   required PRIMARY KEY fields is not present in the database.
** 
**   There is no conflicting row in this case. The results of invoking the
**   sqlite3changeset_conflict() API are undefined.
** 
** <dt>SQLITE_CHANGESET_CONFLICT<dd>
**   CHANGESET_CONFLICT is passed as the second argument to the conflict
**   handler while processing an INSERT change if the operation would result 
**   in duplicate primary key values.
** 
**   The conflicting row in this case is the database row with the matching
**   primary key.
** 
** <dt>SQLITE_CHANGESET_CONSTRAINT<dd>
**   If any other constraint violation occurs while applying a change (i.e. 
**   a FOREIGN KEY, UNIQUE, CHECK or NOT NULL constraint), the conflict 
**   handler is invoked with CHANGESET_CONSTRAINT as the second argument.
** 
**   There is no conflicting row in this case. The results of invoking the
**   sqlite3changeset_conflict() API are undefined.
** </dl>
*/
#define SQLITE_CHANGESET_DATA       1
#define SQLITE_CHANGESET_NOTFOUND   2
#define SQLITE_CHANGESET_CONFLICT   3
#define SQLITE_CHANGESET_CONSTRAINT 4

/* 
** CAPI3REF: Constants Returned By The Conflict Handler
**
** A conflict handler callback must return one of the following three values.
**
** <dl>
** <dt>SQLITE_CHANGESET_OMIT<dd>
**   If a conflict handler returns this value no special action is taken. The
**   change that caused the conflict is not applied. The session module 
**   continues to the next change in the changeset.
**
** <dt>SQLITE_CHANGESET_REPLACE<dd>
**   This value may only be returned if the second argument to the conflict
**   handler was SQLITE_CHANGESET_DATA or SQLITE_CHANGESET_CONFLICT. If this
**   is not the case, any changes applied so far are rolled back and the 
**   call to sqlite3changeset_apply() returns SQLITE_MISUSE.
**
**   If CHANGESET_REPLACE is returned by an SQLITE_CHANGESET_DATA conflict
**   handler, then the conflicting row is either updated or deleted, depending
**   on the type of change.
**
**   If CHANGESET_REPLACE is returned by an SQLITE_CHANGESET_CONFLICT conflict
**   handler, then the conflicting row is removed from the database and a
**   second attempt to apply the change is made. If this second attempt fails,
**   the original row is restored to the database before continuing.
**
** <dt>SQLITE_CHANGESET_ABORT<dd>
**   If this value is returned, any changes applied so far are rolled back 
**   and the call to sqlite3changeset_apply() returns SQLITE_ABORT.
** </dl>
*/
#define SQLITE_CHANGESET_OMIT       0
#define SQLITE_CHANGESET_REPLACE    1
#define SQLITE_CHANGESET_ABORT      2

#endif
