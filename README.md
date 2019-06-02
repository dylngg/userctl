# WIP: userctl
A tool to set persistent and configurable resource controls on users and groups with systemd cgroups.

## Motivation

Using systemd's integration of cgroups into it's notion of a unit, cgroup controls/limits can be set on individual users using the `user-$UID.slice`. This is great for administrators wanting greater control over the amount of resources given to users, but alone, it falls short of the ideal in three main categories.

1. The non-persistent nature of user slices (they are created when a user logs in and destroyed when a user logs out) prevents categorical limits from easily being applied after a user logged out. Newer versions of systemd allow for slice drop-ins\*—files that allow you to set persistent controls on units—but this suffers from the fact that drop-ins must be created for every user on the machine. Furthermore, systemd versions before v242 (Distributions like CentOS 7 or RHEL 7) do not have these drop-ins.

2. There is no automated way to manage or categorize users and their limits. Imagine if you wanted users to have a base limit, but those with a "admin" group membership to have higher limits. Newer verions of systemd allow for a default limit to be applied to all users in a single drop-in\*\* and could be combined specific overriding slice drop-ins, but you still suffer from managing drop-ins for all your users who are not default. Again, you also suffer if you aren't on systemd v242+.

3. These user slices allow per-user resource controls and accounting, but do not enable users to share and account for the same resources, such sharing resources between users in the same group.

_*by adding a specific `user-$UID.slice` [drop-in]()_

_\*\*by adding a `user-.slice` [drop-in]()_

## Solution

`userctl` solves these problems of course! It allows you to create so-called "classes"—a fancy term for a collection of resource control rules—which can be categorically applied to any user, users with a specific unix group or a arbitrary collection of users. It also allows for the optional sharing of the same resources (with a cgroup with a bunch of people in it) within a class.

### Sources
https://github.com/systemd/systemd/issues/2556
