/***************************************************************************
 *   Copyright (C) 2008, 2009 by Malakhov Alexey                           *
 *   brezerk@gmail.com                                                     *
 *                                                                         *
 *   This program is free software: you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   In addition, as a special exception, the copyright holders give       *
 *   permission to link the code of this program with any edition of       *
 *   the Qt library by Trolltech AS, Norway (or with modified versions     *
 *   of Qt that use the same license as Qt), and distribute linked         *
 *   combinations including the two.  You must obey the GNU General        *
 *   Public License in all respects for all of the code used other than    *
 *   Qt.  If you modify this file, you may extend this exception to        *
 *   your version of the file, but you are not obligated to do so.  If     *
 *   you do not wish to do so, delete this exception statement from        *
 *   your version.                                                         *
 ***************************************************************************/

#include "main.h"

corelib::corelib(bool _GUI_MODE)
{
	  // Setting gui mode, if false - cli mode else gui mode
	  this->_GUI_MODE=_GUI_MODE;

	  // Creating databases
	  db_icon = new Icon();
	  db_image = new Image();
	  db_prefix = new Prefix();
}

corelib* createCoreLib(bool _GUI_MODE){
	  return new corelib(_GUI_MODE);
}

QList<QStringList> corelib::getWineProcessList(){
	  QList<QStringList> proclist;
	  QStringList procline;

	  QString name, procstat, path, prefix, env_arg, nice;

#ifdef _OS_LINUX_
	  QString message = "<p>Process is unable access to /proc file system.</p><p>Access is necessary for displaying wine process information.</p><p>You need to set CONFIG_PROC_FS=y option on linux kernel config file and mount proc file system by running: mount -t proc none /proc</p>";
#endif

#ifdef _OS_FREEBSD_
	  QString message = "<p>Process is unable access to /proc file system.</p><p>Access is necessary for displaying wine process information.</p><p>You need to set PSEUDOFS and PROCFS option on FreeBSD kernel config file and mount proc file system by running: mount -t procfs proc /proc</p>";
#endif

	  // Check for /proc directory exists
	  QDir dir("/proc");
	  if (!dir.exists()){
			if (this->showError(message, false) == QMessageBox::Ignore){
				  procline << "-1";
				  proclist << procline;
				  return proclist;
			}
	  }

	  /* On Linux:
	   * This is new engine for getting process info from /proc directory
	   * its fully wrighted with QT and might work more stable =)
	   */
#ifdef _OS_LINUX_
	  dir.setFilter(QDir::Dirs | QDir::NoSymLinks);
	  dir.setSorting(QDir::Name);

	  QFileInfoList list = dir.entryInfoList();

	  // Getting directoryes one by one
	  for (int i = 0; i < list.size(); ++i) {
			QFileInfo fileInfo = list.at(i);
			path = "/proc/";
			path.append(fileInfo.fileName());
			path.append("/stat");
			QFile file(path);
			path = "/proc/";
			path.append(fileInfo.fileName());
			path.append("/exe");
			QFileInfo exelink(path);
			// Try to read stat file
			if (exelink.isSymLink() && (exelink.symLinkTarget().contains("wineserver") || exelink.symLinkTarget().contains("wine-preloader"))){
				  if (file.open(QIODevice::ReadOnly | QIODevice::Text)){
						QTextStream in(&file);
						QString line = in.readLine();
						if (!line.isNull()){
							  // Getting nice and name of the process
							  nice = line.section(' ', 18, 18);
							  name = line.section(' ', 1, 1);
							  name.remove(QChar('('));
							  name.remove(QChar(')'));
							  name = name.toLower();

							  // If name contains wine or .exe and not contains q4wine,
							  // then we try to get environ variables.
							  //if ((name.contains("wine") || name.contains(".exe")) && !name.contains(APP_SHORT_NAME)){
									path = "/proc/";
									path.append(fileInfo.fileName());
									path.append("/environ");
									QFile e_file(path);

									// Getting WINEPREFIX variable.
									if (e_file.open(QIODevice::ReadOnly | QIODevice::Text)){
										  QTextStream e_in(&e_file);
										  QString e_line = e_in.readLine();
										  int index = e_line.indexOf("WINEPREFIX=");
										  prefix="";
										  if (index!=-1)
												for (int i=index+11; i<=e_line.length(); i++){
												if (e_line.mid(i, 1).data()->isPrint()){
													  prefix.append(e_line.mid(i, 1));
												} else {
													  break;
												}
										  }
									}

									// Puting all fields into QList<QStringList>
									procline.clear();
									procline << fileInfo.fileName() << name << nice << prefix;
									proclist << procline;

							  //}
						}
						file.close();
				  }
			}
	  }
#endif

	  /* On FreeBSD:
		 * This is new engine for getting process info from /proc directory and kmem interface
		 */
#ifdef _OS_FREEBSD_
	  kvm_t *kd;
	  int cntproc, i, ni, ipid;

	  struct kinfo_proc *kp;
	  char buf[_POSIX2_LINE_MAX];
	  char **envs;

	  kd = kvm_openfiles(_PATH_DEVNULL, _PATH_DEVNULL, NULL, O_RDONLY, buf);
	  if (!kd){
			if (this->showError(QObject::tr("<p>It seems q4wine can not run kvm_openfiles.</p>"), false) == QMessageBox::Ignore){
				  procline << "-1";
				  proclist << procline;
				  return proclist;
			}
			return proclist;
	  }
	  kp = kvm_getprocs(kd, KERN_PROC_ALL, 0, &cntproc);
	  if (!kp){
			if (this->showError(QObject::tr("<p>It seems q4wine can not run kvm_getprocs.</p>"), false) == QMessageBox::Ignore){
				  procline << "-1";
				  proclist << procline;
				  return proclist;
			}
			return proclist;
	  }

	  QStringList cur_pids;
	  for (i=0; i<cntproc;i++){
			prefix="";
			ipid = kp[i].ki_pid;

			if (cur_pids.indexOf(QObject::tr("%1").arg(ipid))==-1){
				  cur_pids << QObject::tr("%1").arg(ipid);
				  name = kp[i].ki_comm;

				  if ((name.contains("wine") || name.contains(".exe")) && !name.contains(APP_SHORT_NAME)){
						ni = kp[i].ki_nice;
						nice = QObject::tr("%1").arg(ni);

						if (name.contains("pthread")){
							  envs = kvm_getargv(kd, (const struct kinfo_proc *) &(kp[i]), 0);
							  if (envs){
									name = envs[0];
									if (name.isEmpty()){
										  name = kp[i].ki_comm;
									} else {
										  name = name.split('\\').last();
									}
							  }
						}

						envs = kvm_getenvv(kd, (const struct kinfo_proc *) &(kp[i]), 0);
						if (envs){
							  int j=0;
							  while (envs[j]){
									env_arg=envs[j];
									int index = env_arg.indexOf("WINEPREFIX=");
									if (index>=0){
										  prefix=env_arg.mid(11);
										  break;
									}
									j++;
							  }
						} else {
							  prefix="";
						}

						// Puting all fields into QList<QStringList>
						procline.clear();
						procline << QObject::tr("%1").arg(ipid) << name << nice << prefix;
						proclist << procline;
				  }
			}
	  }
#endif


	  return proclist;
}

QVariant corelib::getSetting(const QString group, const QString key, const bool checkExist, const QVariant defaultVal) const{
	  QVariant retVal;
	  QSettings settings(APP_SHORT_NAME, "default");

	  settings.beginGroup(group);
	  retVal = settings.value(key, defaultVal);
	  settings.endGroup();
	  if (checkExist==true)
			if (!QFileInfo(retVal.toString()).exists()){
			this->showError(QObject::tr("<p>Error while loading application settings by key: '%1'. File or path not exists: \"%2\"</p><p>Please, go to %3 options dialog and set it.</p>").arg(key).arg(retVal.toString()).arg(APP_SHORT_NAME));
			retVal = QVariant();
	  }
	  return retVal;
}

QStringList corelib::getCdromDevices(void) const{
	  QStringList retVal;

	  QFile file("/etc/fstab");
	  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)){
			this->showError(QObject::tr("Sorry, i can't access to /etc/fstab"));
	  }

	  retVal<<QObject::tr("<none>");

	  while (1) {
			QByteArray line = file.readLine();

			if (line.isEmpty())
				  break;

			if (line.indexOf("cdrom")>=0){
				  QList<QByteArray> array = line.split(' ').at(0).split('\t');
				  retVal<<array.at(0);
			}
	  }

	  file.close();

	  return retVal;
}

QStringList corelib::getWineDlls(QString prefix_lib_path) const{
	  QStringList dllList;
	  if (prefix_lib_path.isEmpty()){
			prefix_lib_path=this->getSetting("wine", "WineLibs").toString();
	  }

	  QDir dir(prefix_lib_path);
	  dir.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);

	  QFileInfoList list = dir.entryInfoList();
	  for (int i = 0; i < list.size(); ++i) {
			QFileInfo fileInfo = list.at(i);
			if (fileInfo.fileName().indexOf(".dll.so")>=0){
				  dllList<<fileInfo.fileName().left(fileInfo.fileName().length()-3);
			}
	  }

	  return dllList;
}



QString corelib::getWhichOut(const QString fileName) const{
	  /*
	   * Getting 'which' output;
	   */

	  QProcess *proc;

	  proc = new QProcess();

	  QStringList args;

	  args<<fileName;

	  proc->setWorkingDirectory (QDir::homePath());
	  proc->start("which", args, QIODevice::ReadOnly);
	  proc->waitForFinished();

	  QString string = proc->readAllStandardOutput();
	  delete proc;

	  if (!string.isEmpty()){
			return string.trimmed();
	  } else {
			this->showError(QObject::tr("Can't find or execute '%1' binary. See INSTALL file for application depends.").arg(fileName));
	  }

	  return "";
}

QString corelib::getMountedImages(const QString cdrom_mount) const{
	  QString image="";
	  QStringList arguments;

#ifdef _OS_LINUX_
	  arguments << "-c" << QObject::tr("%1 | grep %2").arg(this->getSetting("system", "mount").toString()).arg(cdrom_mount);
#endif
#ifdef _OS_FREEBSD_
	  arguments << "-c" << QObject::tr("%1 | grep %2").arg(this->getSetting("system", "mount").toString()).arg(cdrom_mount);
#endif

	  QProcess *myProcess = new QProcess();
	  myProcess->start(this->getSetting("system", "sh").toString(), arguments);
	  if (!myProcess->waitForFinished()){
			qDebug() << "Make failed:" << myProcess->errorString();
			return QString();
	  }

	  image = myProcess->readAll();
	  if (!image.isEmpty()){
			image = image.split(" ").first();
			if (!image.isEmpty()){
#ifdef _OS_LINUX_
				  if (image.contains("loop")){
#endif
#ifdef _OS_FREEBSD_
						if (image.contains("md")){
#endif
							  myProcess->close ();
							  arguments.clear();
#ifdef _OS_LINUX_
							  arguments << "losetup" << image;
#endif
#ifdef _OS_FREEBSD_
							  arguments << "mdconfig" <<  "-l" << QObject::tr("-u%1").arg(image.mid(7));
#endif
							  myProcess->start(this->getSetting("system", "sudo").toString(), arguments);
							  if (!myProcess->waitForFinished()){
									qDebug() << "Make failed:" << myProcess->errorString();
									return QString();
							  } else {
									image = myProcess->readAll();
#ifdef _OS_LINUX_
									image = image.split("/").last().mid(0, image.split("/").last().length()-2);
#endif
#ifdef _OS_FREEBSD_
									image = image.split("/").last().mid(0, image.split("/").last().length()-1);
#endif
							  }
						}
				  } else {
						image = "none";
				  }
			} else {
				  image = "none";
			}
			return image;
	  }

	  bool corelib::runIcon(const QString prefix_name, const QString dir_name, const QString icon_name) const{
			QStringList result = db_icon->getByName(prefix_name, dir_name, icon_name);
			//  0   1     2     3          4       5         6          7           8        9        10    11       12    13         14
			//	id, name, desc, icon_path, wrkdir, override, winedebug, useconsole, display, cmdargs, exec, desktop, nice, prefix_id, dir_id
			ExecObject execObj;
			execObj.wrkdir = result.at(4);
			execObj.override = result.at(5);
			execObj.winedebug = result.at(6);
			execObj.useconsole = result.at(7);
			execObj.display = result.at(8);
			execObj.cmdargs = result.at(9);
			execObj.execcmd = result.at(10);
			execObj.desktop = result.at(11);
			execObj.nice = result.at(12);
			execObj.prefixid = result.at(13);
			return runWineBinary(execObj);
	  }

	  bool corelib::runWineBinary(const ExecObject execObj) const{
			QStringList prefixList;
			// 0   1     2             3            4            5          6            7
			// id, path, wine_dllpath, wine_loader, wine_server, wine_exec, cdrom_mount, cdrom_drive
			prefixList = db_prefix->getFieldsByPrefixId(execObj.prefixid);

			QString exec;
			QStringList args;
			QString envargs;

			if (execObj.useconsole == "1"){
				  // If we gona use console output, so exec program is program specificed at CONSOLE global variable
				  exec = this->getSetting("console", "bin").toString();

				  if (!this->getSetting("console", "args", false).toString().isEmpty()){
						// If we have any conslope parametres, we gona preccess them one by one
						QStringList cons_args = this->getSetting("console", "args", false).toString().split(" ");
						for (int i=0; i<cons_args.count(); i++){
							  if (!cons_args.at(i).isEmpty())
									args.append(cons_args.at(i));
						}
				  }

				  args.append(this->getSetting("system", "sh").toString());

			} else {
				  exec = this->getSetting("system", "sh").toString();
			}

			args.append("-c");


			if ((execObj.useconsole == "1") && (!execObj.wrkdir.isNull())){
				  envargs.append(" cd '");
				  envargs.append(execObj.wrkdir);
				  envargs.append("' && ");
			}

			if (!prefixList.at(1).isEmpty()){
				  //If icon has prefix -- add to args
				  envargs.append(QObject::tr(" WINEPREFIX=%1 ").arg(prefixList.at(1)));
			} else {
				  //Else use default prefix
				  envargs.append(QObject::tr(" WINEPREFIX=%1/.wine ").arg(QDir::homePath()));
			}

			if (!prefixList.at(2).isEmpty()){
				  envargs.append(QObject::tr(" WINEDLLPATH=%1 ").arg(prefixList.at(2)));
			} else {
				  envargs.append(QObject::tr(" WINEDLLPATH=%1 ").arg(this->getSetting("wine", "WineLibs").toString()));
			}

			if (!prefixList.at(3).isEmpty()){
				  envargs.append(QObject::tr(" WINELOADER=%1 ").arg(prefixList.at(3)));
			} else {
				  envargs.append(QObject::tr(" WINELOADER=%1 ").arg(this->getSetting("wine", "LoaderBin").toString()));
			}

			if (!prefixList.at(4).isEmpty()){
				  envargs.append(QObject::tr(" WINESERVER=%1 ").arg(prefixList.at(4)));
			} else {
				  envargs.append(QObject::tr(" WINESERVER=%1 ").arg(this->getSetting("wine", "ServerBin").toString()));
			}

			if (!execObj.override.isEmpty()){
				  envargs.append(QObject::tr(" WINEDLLOVERRIDES='%1' ").arg(execObj.override));
			}

			if (!execObj.winedebug.isEmpty() && execObj.useconsole == "1"){
				  envargs.append(QObject::tr(" WINEDEBUG=%1 ").arg(execObj.winedebug));
			}

			if (!execObj.display.isEmpty()){
				  envargs.append(QObject::tr(" DISPLAY=%1 ").arg(execObj.display));
			}

			QString exec_string = "";

			exec_string.append(envargs);
			if(!execObj.nice.isEmpty()){
				  exec_string.append(this->getSetting("system", "nice").toString());
				  exec_string.append(" -n ");
				  exec_string.append(execObj.nice);
			}

			exec_string.append(" ");

			if (!prefixList.at(5).isEmpty()){
				  exec_string.append(prefixList.at(5));
			} else {
				  exec_string.append(this->getSetting("wine", "WineBin").toString());
			}

			exec_string.append(" ");

			if (!execObj.desktop.isEmpty()){
				  exec_string.append(" explorer.exe /desktop=");
				  exec_string.append(execObj.desktop);
			}

			exec_string.append(" \"");
			exec_string.append(execObj.execcmd);
			exec_string.append("\" ");
			exec_string.append(execObj.cmdargs);

			args.append(exec_string);

			QProcess *proc;
			proc = new QProcess();
			return proc->startDetached( exec, args, execObj.wrkdir );
	  }


	  bool corelib::runWineBinary(const QString winebinary, const QString cmdargs, const QString prefix_name) const{
			QStringList prefixList;
			// 0   1     2             3            4            5          6            7
			// id, path, wine_dllpath, wine_loader, wine_server, wine_exec, cdrom_mount, cdrom_drive
			prefixList = db_prefix->getFieldsByPrefixName(prefix_name);

			QString exec;
			QStringList args;
			QString envargs;

			exec = this->getSetting("system", "sh").toString();

			args.append("-c");

			if (!prefixList.at(1).isEmpty()){
				  //If icon has prefix -- add to args
				  envargs.append(QObject::tr(" WINEPREFIX=%1 ").arg(prefixList.at(1)));
			} else {
				  //Else use default prefix
				  envargs.append(QObject::tr(" WINEPREFIX=%1/.wine ").arg(QDir::homePath()));
			}

			if (!prefixList.at(2).isEmpty()){
				  envargs.append(QObject::tr(" WINEDLLPATH=%1 ").arg(prefixList.at(2)));
			} else {
				  envargs.append(QObject::tr(" WINEDLLPATH=%1 ").arg(this->getSetting("wine", "WineLibs").toString()));
			}

			if (!prefixList.at(3).isEmpty()){
				  envargs.append(QObject::tr(" WINELOADER=%1 ").arg(prefixList.at(3)));
			} else {
				  envargs.append(QObject::tr(" WINELOADER=%1 ").arg(this->getSetting("wine", "LoaderBin").toString()));
			}

			if (!prefixList.at(4).isEmpty()){
				  envargs.append(QObject::tr(" WINESERVER=%1 ").arg(prefixList.at(4)));
			} else {
				  envargs.append(QObject::tr(" WINESERVER=%1 ").arg(this->getSetting("wine", "ServerBin").toString()));
			}

			QString exec_string = "";
			exec_string.append(envargs);
			exec_string.append(" ");

			if (!prefixList.at(5).isEmpty()){
				  exec_string.append(prefixList.at(5));
			} else {
				  exec_string.append(this->getSetting("wine", "WineBin").toString());
			}
			exec_string.append(" ");
			exec_string.append(" \"");
			exec_string.append(winebinary);
			exec_string.append("\" ");
			exec_string.append(cmdargs);

			args.append(exec_string);

			QProcess *proc;
			proc = new QProcess();
			return proc->startDetached(exec, args, QDir::homePath());
	  }

	  QString corelib::createDesktopFile(const QString prefix_name, const QString dir_name, const QString icon_name) const{
			QStringList result = db_icon->getByName(prefix_name, dir_name, icon_name);

			QString fileName = QDir::homePath();
			fileName.append("/.config/");
			fileName.append(APP_SHORT_NAME);
			fileName.append("/tmp/");
			fileName.append(result.at(1));
			fileName.append(".desktop");

			QFile file(fileName);
			file.open(QIODevice::Truncate | QIODevice::WriteOnly);

			QTextStream out(&file);
			out<<"[Desktop Entry]"<<endl;
			out<<"Exec=q4wine-cli -p \""<<prefix_name<<"\" ";
			if (!dir_name.isEmpty())
				  out<<" -d \""<<dir_name<<"\" ";
			out<<" -i \""<<icon_name<<"\" "<<endl;

			if (result.at(3).isEmpty()){
				  out<<"Icon="<<APP_PREF<<"/share/q4wine/icons/exec_wine.png"<<endl;
			} else {
				if (result.at(3)=="winecfg"){
					out<<"Icon="<<APP_PREF<<"/share/q4wine/icons/winecfg.png"<<endl;
				} else if (result.at(3)=="wineconsole"){
					out<<"Icon="<<APP_PREF<<"/share/q4wine/icons/wineconsole.png"<<endl;
				} else if (result.at(3)=="uninstaller"){
					out<<"Icon="<<APP_PREF<<"/share/q4wine/icons/uninstaller.png"<<endl;
				} else if (result.at(3)=="regedit"){
					out<<"Icon="<<APP_PREF<<"/share/q4wine/icons/regedit.png"<<endl;
				} else if (result.at(3)=="explorer"){
					out<<"Icon="<<APP_PREF<<"/share/q4wine/icons/explorer.png"<<endl;
				} else if (result.at(3)=="eject"){
					out<<"Icon="<<APP_PREF<<"/share/q4wine/icons/eject.png"<<endl;
				} else if (result.at(3)=="wordpad"){
					out<<"Icon="<<APP_PREF<<"/share/q4wine/icons/notepad.png"<<endl;
				} else {
					out<<"Icon="<<result.at(3)<<endl;
				}
			}
			out<<"Type=Application"<<endl;
			out<<"X-KDE-StartupNotify=true"<<endl;
			out<<"GenericName="<<result.at(2)<<endl;
			out<<"Name="<<result.at(2)<<endl;
			out<<"Path="<<result.at(4)<<endl;

			file.close();

			return fileName;
	  }


	  bool corelib::mountImage(const QString image_name, const QString prefix_name) const{
			QString mount_point=db_prefix->getFieldsByPrefixName(prefix_name).at(6);

			if (mount_point.isEmpty()){
				  this->showError(QObject::tr("It seems no mount point was set in prefix options.<br>You might need to set it manualy."));
				  return FALSE;
			}
			if (image_name.isEmpty())
				  return FALSE;

			QStringList args;
			QString mount_string;

#ifdef _OS_FREEBSD_
			if (image_name.contains("/")) {
				  mount_string=this->getSetting("advanced", "mount_drive_string", false).toString();
				  mount_string.replace("%MOUNT_DRIVE%", image_name);
			} else {
				  mount_string=this->getSetting("advanced", "mount_image_string", false).toString();
				  mount_string.replace("%MOUNT_IMAGE%", this->db_image->getPath(image_name));
				  mount_string.replace("%MDCONFIG_BIN%", getWhichOut("mdconfig"));
			}

			mount_string.replace("%GUI_SUDO%", getSetting("system", "gui_sudo").toString());
			mount_string.replace("%MOUNT_BIN%", getSetting("system", "mount").toString());
			mount_string.replace("%MOUNT_POINT%", mount_point);
#endif

#ifdef _OS_LINUX_
			if ((image_name.contains("/") && (!image_name.contains(".iso")) && (!image_name.contains(".nrg")))) {
				  mount_string=this->getSetting("advanced", "mount_drive_string", false).toString();
				  mount_string.replace("%MOUNT_DRIVE%", image_name);
			} else {
				mount_string=this->getSetting("advanced", "mount_image_string", false).toString();

				  if (!QFile(image_name).exists()){
						mount_string.replace("%MOUNT_IMAGE%", this->db_image->getPath(image_name));
				  } else {
						mount_string.replace("%MOUNT_IMAGE%", image_name);
				  }

				  mount_string.replace("%MOUNT_IMAGE%", this->db_image->getPath(image_name));
				  if (image_name.right(3)=="nrg"){
						mount_string.replace("%MOUNT_OPTIONS%", "-o  loop,offset=307200");
				  } else {
						mount_string.replace("%MOUNT_OPTIONS%", "-o  loop");
				  }
			}

			mount_string.replace("%GUI_SUDO%", getSetting("system", "gui_sudo").toString());
			mount_string.replace("%MOUNT_BIN%", getSetting("system", "mount").toString());
			mount_string.replace("%MOUNT_POINT%", mount_point);
#endif

			args.clear();
			args.append("-c");
			args.append(mount_string);


			if (this->_GUI_MODE){
				  Process *proc;
				  proc = new Process(args, this->getSetting("system", "sh").toString(), QDir::homePath(), QObject::tr("Mounting %1 into %2").arg(image_name).arg(mount_point), QObject::tr("Mounting..."));
				  if (proc->exec()==QDialog::Accepted){
					 return TRUE;
				 } else {
				  return FALSE;
				 }
			} else {
				  return (this->runProcess(this->getSetting("system", "sh").toString(), args));
			}
	  }

   bool corelib::umountImage(const QString prefix_name) const{
			QString mount_point=db_prefix->getFieldsByPrefixName(prefix_name).at(6);

			if (mount_point.isEmpty()){
				  this->showError(QObject::tr("It seems no mount point was set in prefix options.<br>You might need to set it manualy."));
				  return FALSE;
			}

			QStringList args;
			QString arg;

#ifdef _OS_FREEBSD_
			args.clear();
			args << "-c" << tr("%1 | grep %2").arg(this->getSetting("system", "mount").toString()).arg(mount_point);

			QProcess *myProcess = new QProcess(this);
			myProcess->start(this->getSetting("system", "sh").toString(), args);
			if (!myProcess->waitForFinished()){
				  qDebug() << "Make failed:" << myProcess->errorString();
				  return;
			}

			QString devid = myProcess->readAll();
#endif

			QString mount_string;
			mount_string=this->getSetting("advanced", "umount_string", false).toString();
			mount_string.replace("%GUI_SUDO%", getSetting("system", "gui_sudo").toString());
			mount_string.replace("%UMOUNT_BIN%", getSetting("system", "umount").toString());
			mount_string.replace("%MOUNT_POINT%", mount_point);

			args.clear();
			args.append("-c");
			args.append(mount_string);

			if (this->_GUI_MODE){
				  Process *proc;
				  proc = new Process(args, this->getSetting("system", "sh").toString(), QDir::homePath(), QObject::tr("Mounting..."), QObject::tr("Mounting..."));
				  return (proc->exec());
			} else {
				  return (this->runProcess(this->getSetting("system", "sh").toString(), args));
			}

#ifdef _OS_FREEBSD_
			if (!devid.isEmpty()){
				  devid = devid.split(" ").first();
				  if (!devid.isEmpty()){
						if (devid.contains("md")){
							  args.clear();
							  args << "mdconfig" <<  "-d" << tr("-u%1").arg(devid.mid(7));

							  //Fix args for kdesu\gksu\e.t.c.
							  if (!this->getSetting("system", "gui_sudo").toString().contains(QRegExp("/sudo$"))){
									arg=args.join(" ");
									args.clear();
									args<<arg;
							  }

							  if (this->_GUI_MODE){
									Process *proc;
									proc = new Process(args, this->getSetting("system", "gui_sudo").toString(), QDir::homePath(), QObject::tr("Mounting..."), QObject::tr("Mounting..."));
									return (proc->exec());
							  } else {
									return (this->runProcess(this->getSetting("system", "gui_sudo").toString(), args));
							  }
						}
				  }
			}
#endif

			return TRUE;
	  }

	  bool corelib::openIconDirectry(const QString prefix_name, const QString dir_name, const QString icon_name) const{
			QStringList result = db_icon->getByName(prefix_name, dir_name, icon_name);
			QStringList args;
			args<<result.at(4);
			return this->runProcess(this->getWhichOut("xdg-open"), args, "", FALSE);
	  }

	  bool corelib::openPrefixDirectry(const QString prefix_name) const{
			QStringList args;
			args<<db_prefix->getPath(prefix_name);
			return this->runProcess(this->getWhichOut("xdg-open"), args, "", FALSE);
	  }


	  QString corelib::getWinePath(const QString path, const QString option) const{
			QString output, exec;
			QStringList args;

			args.append(option);
			args.append(path);
			exec = this->getWhichOut("winepath");

			QProcess *myProcess;
			myProcess = new QProcess();
			myProcess->setEnvironment(QProcess::systemEnvironment());
			myProcess->setWorkingDirectory (QDir::homePath());
			myProcess->start(exec, args);

			if (myProcess->waitForFinished()){
				  output = myProcess->readAllStandardOutput().trimmed();
			}
			return output;
	  }

	  bool corelib::runProcess(const QString exec, const QStringList args, QString dir, bool showLog) const{
			if (dir.isEmpty())
				  dir=QDir::homePath();

			QProcess *myProcess;
			myProcess = new QProcess();
			myProcess->setEnvironment(QProcess::systemEnvironment());
			myProcess->setWorkingDirectory (dir);
			myProcess->start(exec, args);

			if (!myProcess->waitForFinished())
				  return FALSE;

			if (showLog){
				  // Getting env LANG variable
				  QString lang=getenv("LANG");
				  lang=lang.split(".").at(1);

				  // If in is empty -- set UTF8 locale
				  if (lang.isEmpty())
						lang = "UTF8";

				  // Read STDERR with locale support
				  QTextCodec *codec = QTextCodec::codecForName(lang.toAscii());
				  QString string = codec->toUnicode(myProcess->readAllStandardError());

				  if (!string.isEmpty()){
						showError(QObject::tr("It seems the process crashed. STDERR log: %1").arg(string));
						return FALSE;
				  }
			}
			return TRUE;
	  }

	  int corelib::showError(const QString message, const bool info) const{
			switch (this->_GUI_MODE){
			case true:
				  switch (info){
				  case true:
						QMessageBox::warning(0, QObject::tr("Error"), message);
						return 0;
						break;
				  case false:
						return QMessageBox::warning(0, QObject::tr("Error"), message, QMessageBox::Retry, QMessageBox::Ignore);
						break;
				  }
				  break;
			case false:
				  cout << "test"; // message.toLatin1();
				  break;
			}
			return 0;
	  }

	  void corelib::showError(const QString message) const{
			QTextStream Qcout(stdout);
			switch (this->_GUI_MODE){
			case true:
				  QMessageBox::warning(0, QObject::tr("Error"), message);
				  break;
			case false:
				  Qcout<<QObject::tr("Error")<<endl<<message<<endl;
				  break;
			}
			return;
	  }

	  bool corelib::killWineServer(const QString prefix_path) const{
			QString command;

			if (!prefix_path.isEmpty()){
				  command=QObject::tr("env WINEPREFIX=\"%1\" wineserver -kill").arg(prefix_path);
			} else {
				  command="wineserver -kill";
			}


			if (system(command.toAscii().data())==-1){
				  this->showError(QObject::tr("Can't run: %1").arg(command.toAscii().data()));
				  return FALSE;
			}

			return TRUE;
	  }
