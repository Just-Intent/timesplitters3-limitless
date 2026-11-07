"""The GitHub repository this checkout publishes releases from.

Keep in sync with ReleaseClient.Repository (tools/launcher/ReleaseClient.cs) and
config/project-repository.json (PowerShell). UPSTREAM_REPOSITORY is credit and
attribution for the original project only; it is never a release source.
tools/ci/compatibility-worker.py is installed as a standalone copy, so it keeps
its own matching constant.
"""

REPOSITORY = 'Just-Intent/timesplitters3-limitless'
UPSTREAM_REPOSITORY = 'TheDoffman/timesplitters-future-perfect-pc'
URL = 'https://github.com/' + REPOSITORY
ISSUES_URL = URL + '/issues'
RELEASES_URL = URL + '/releases'
UPSTREAM_URL = 'https://github.com/' + UPSTREAM_REPOSITORY
