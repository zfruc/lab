
#undef NO_REAL_DISK_IO

#undef NO_EXTRA_SSD_IO
#undef NO_CACHE

#undef REPORT_MONITOR  //decide whether to report count of handled trace & hit rate or not

#define SHORTEST_USER_DECIDE //if someone finish, others exit as soon as possible

#undef CG_THROTTLE     // CGroup throttle.
#define MULTIUSER
/**< Statistic information requirments defination */
#undef  LOG_ALLOW
#undef  LOG_SINGLE_REQ  // Print detail time information of each single request.

/** Simulator Related **/
#undef SIMULATION
#undef SIMULATOR_AIO


