"""Prepares found differences for visualisation and runs the result viewer"""
from diffkemp.syndiff.function_syntax_diff import unified_syntax_diff
from http.server import HTTPServer, SimpleHTTPRequestHandler
from diffkemp.utils import CMP_OUTPUT_FILE
import sys
import os
import errno
import yaml
import shutil


VIEW_INSTALL_DIR = "/var/lib/diffkemp/view"


def view(args):
    """
    View the compare differences. Prepares files for the visualisation
    and runs viewer.
    """
    viewer = Viewer(compare_output_dir=args.compare_output_dir,
                    devel=args.devel)
    viewer.run()


class Viewer:
    """Class for running the result viewer (viewer of found differences)."""
    def __init__(self, compare_output_dir, devel):
        """
        :param compare_output_dir: Dir containing output of compare command.
        :param devel: True to run viewer in development mode.
        """
        self.compare_output_dir = compare_output_dir
        self.devel = devel
        # Relative path of src files which were already copied to viewer dir.
        self.old_processed_files = set()
        self.new_processed_files = set()
        # Path to snapshot directories.
        self.old_snapshot_dir = None
        self.new_snapshot_dir = None
        # Loaded content of YAML which describes the found differences.
        self.yaml_result = None
        # Path to the viewer.
        self.view_dir = None
        # Path to the directory which viewer can access.
        self.public_dir = None
        # Path to the viewer dir containing data of the compared project.
        self.data_dir = None
        # Path to the viewer dir containing sources of the compared project.
        self.old_out_dir = None
        self.new_out_dir = None
        # Path to the viewer dir containing diff of functions.
        self.diff_dir = None
        # Path to the yaml (describing the result) in the viewer directory.
        self.viewer_yaml_path = None

    def run(self):
        """Prepare and run the viewer."""
        self._load_yaml()
        self._get_snapshot_dirs()
        self._get_viewer_dir()
        self._prepare_dirs()
        self._prepare_files_and_diffs()
        self._start_server()
        self._clean()

    def _load_yaml(self):
        """Load yaml which describes results."""
        yaml_path = os.path.join(self.compare_output_dir, CMP_OUTPUT_FILE)
        if not os.path.exists(yaml_path):
            sys.stderr.write(
                f"ERROR: compare output is missing file "
                f"'{CMP_OUTPUT_FILE}'\n")
            sys.exit(errno.EINVAL)
        with open(yaml_path, "r") as file:
            self.yaml_result = yaml.safe_load(file)

    def _get_viewer_dir(self):
        """Determine the viewer directory.
        The manually built one has priority over the installed one."""
        self.view_dir = os.path.join(os.path.dirname(__file__), "../view")
        # Manually build one
        if os.path.exists(self.view_dir):
            if self.devel:
                self.public_dir = os.path.join(self.view_dir, "public")
            else:
                self.public_dir = os.path.join(self.view_dir, "build")
                if not os.path.isdir(self.public_dir):
                    sys.stderr.write(
                        "Could not find production build of the viewer.\n"
                        "Use --devel to run a development server "
                        "or execute CMake with -DBUILD_VIEWER=On.\n")
                    sys.exit(errno.ENOENT)
        # Installed one
        elif os.path.exists(VIEW_INSTALL_DIR):
            self.view_dir = VIEW_INSTALL_DIR
            self.public_dir = VIEW_INSTALL_DIR
            if self.devel:
                sys.stderr.write(
                    "Error: it is not possible to run the development server "
                    "for installed DiffKemp\n")
                sys.exit(errno.EINVAL)
        # View directory does not exist
        else:
            sys.stderr.write(
                "Error: the viewer was not found.\n"
                "The viewer was probably not installed.\n")
            sys.exit(errno.ENOENT)

    def _prepare_dirs(self):
        """Set and create subdirectories in the viewer directory."""
        # Dir for storing necessary data for a visualisation.
        self.data_dir = os.path.join(self.public_dir, "data")
        if not os.path.exists(self.data_dir):
            os.makedirs(self.data_dir)
        # Preparing source directories
        self.old_out_dir = os.path.join(self.data_dir, "src-old")
        self.new_out_dir = os.path.join(self.data_dir, "src-new")
        if not os.path.exists(self.old_out_dir):
            os.mkdir(self.old_out_dir)
        if not os.path.exists(self.new_out_dir):
            os.mkdir(self.new_out_dir)
        # Preparing diff directory
        self.diff_dir = os.path.join(self.data_dir, "diffs")
        if not os.path.exists(self.diff_dir):
            os.mkdir(self.diff_dir)

    def _get_snapshot_dirs(self):
        """Extract path to snapshot directories from the YAML."""
        self.old_snapshot_dir = self.yaml_result["old-snapshot"]
        self.new_snapshot_dir = self.yaml_result["new-snapshot"]
        # Check if snapshot dirs exist
        if not os.path.isdir(self.old_snapshot_dir):
            sys.stderr.write("Error: expecting to find old snapshot in "
                             f"{self.old_snapshot_dir}\n")
            sys.exit(errno.EINVAL)
        if not os.path.isdir(self.new_snapshot_dir):
            sys.stderr.write("Error: expecting to find new snapshot in "
                             f"{self.new_snapshot_dir}\n")
            sys.exit(errno.EINVAL)

    def _prepare_files_and_diffs(self):
        """Prepare source and diff files to viewer directory."""
        for name, definition in self.yaml_result["definitions"].items():
            self._prepare_file_and_diff(name, definition)
        # Save updated YAML to viewer directory.
        self.viewer_yaml_path = os.path.join(self.data_dir, CMP_OUTPUT_FILE)
        with open(self.viewer_yaml_path, "w") as file:
            yaml.dump(self.yaml_result, file, sort_keys=False)

    def _prepare_file_and_diff(self, name, definition):
        """
        If possible create diff for symbol specified by `name` and described by
        `definition`, update the YAML with info about the diff existence and
        save the source file containing the symbol and the diff to viewer dir.
        """
        # Relatives paths to source files, None if we do not know it.
        old_file = None
        new_file = None
        if "old" in definition:
            old_file = definition["old"]["file"]
            self._copy_file(old_file, is_old=True)
        if "new" in definition:
            new_file = definition["new"]["file"]
            self._copy_file(new_file, is_old=False)

        if (old_file is None or new_file is None or
                "end-line" not in definition["old"] or
                "end-line" not in definition["new"]):
            definition["diff"] = False
            return

        # Make diff of function, add diff info to YAML
        old_file_abs_path = os.path.join(self.old_snapshot_dir, old_file)
        new_file_abs_path = os.path.join(self.new_snapshot_dir, new_file)
        diff = unified_syntax_diff(
            first_file=old_file_abs_path,
            second_file=new_file_abs_path,
            first_line=definition["old"]["line"],
            second_line=definition["new"]["line"],
            first_end=definition["old"]["end-line"],
            second_end=definition["new"]["end-line"])
        if diff.isspace() or diff == "":
            definition["diff"] = False
        else:
            definition["diff"] = True
            diff_path = os.path.join(self.diff_dir, name + ".diff")
            with open(diff_path, "w") as file:
                file.write(diff)

    def _copy_file(self, file_path, is_old=True):
        """
        Copy file to out_src_dir if it is not already processed.
        :param file_path: Relative path to snapshot dir of a file to copy.
        :param is_old: True for old version, false for new version of project.
        """
        if is_old:
            file_abs_path = os.path.join(self.old_snapshot_dir, file_path)
            processed_files = self.old_processed_files
            out_dir = self.old_out_dir
        else:
            file_abs_path = os.path.join(self.new_snapshot_dir, file_path)
            processed_files = self.new_processed_files
            out_dir = self.new_out_dir
        if file_path in processed_files:
            return
        processed_files.add(file_path)
        output_file_path = os.path.join(out_dir, file_path)
        # Copying directory hierarchy.
        output_dir_path = os.path.dirname(output_file_path)
        os.makedirs(output_dir_path, exist_ok=True)
        # Copying file.
        shutil.copy(file_abs_path, output_dir_path)

    def _start_server(self):
        """Start server with the viewer."""
        if self.devel:
            os.chdir(self.view_dir)
            os.system("npm install")
            os.system("npm start")
        else:
            os.chdir(self.public_dir)
            handler = SimpleHTTPRequestHandler
            handler.log_message = lambda *_, **__: None
            with HTTPServer(("localhost", 3000), handler) as httpd:
                print("Result viewer is available at http://localhost:3000")
                print("Press Ctrl+C to exit")
                try:
                    httpd.serve_forever()
                except KeyboardInterrupt:
                    httpd.shutdown()

    def _clean(self):
        """Clean directories."""
        shutil.rmtree(self.old_out_dir)
        shutil.rmtree(self.new_out_dir)
        shutil.rmtree(self.diff_dir)
        os.remove(self.viewer_yaml_path)
