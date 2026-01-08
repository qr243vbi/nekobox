#
# spec file for package [spectemplate]
#
# Copyright (c) 2026 SUSE LLC
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via https://bugs.opensuse.org/
#


Name:           
Version:        
Release:        0
Summary:        
License:        
URL:            
Source0:        
BuildRequires:  
Requires:       

%description


%prep
%setup -q

%build
%configure
make %{?_smp_mflags}

%install
%make_install

%files
%license add-license-file-here
%doc add-docs-here

%changelog
