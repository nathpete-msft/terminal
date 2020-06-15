// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <LibraryResources.h>
#include "ColorPickupFlyout.h"
#include "Tab.h"
#include "Tab.g.cpp"
#include "Utils.h"
#include "ColorHelper.h"

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace winrt::Windows::System;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
}

namespace winrt::TerminalApp::implementation
{
    Tab::Tab(const GUID& profile, const TermControl& control)
    {
        _rootPane = std::make_shared<Pane>(profile, control, true);

        _rootPane->Closed([=](auto&& /*s*/, auto&& /*e*/) {
            _ClosedHandlers(nullptr, nullptr);
        });

        _activePane = _rootPane;
    }

    // Method Description:
    // - Get the root UIElement of this Tab's root pane.
    // Arguments:
    // - <none>
    // Return Value:
    // - The UIElement acting as root of the Tab's root pane.
    UIElement Tab::GetRootElement()
    {
        return _rootPane->GetRootElement();
    }

    // Method Description:
    // - Returns nullptr if no children of this tab were the last control to be
    //   focused, or the TermControl that _was_ the last control to be focused (if
    //   there was one).
    // - This control might not currently be focused, if the tab itself is not
    //   currently focused.
    // Arguments:
    // - <none>
    // Return Value:
    // - nullptr if no children were marked `_lastFocused`, else the TermControl
    //   that was last focused.
    TermControl Tab::GetActiveTerminalControl() const
    {
        return _activePane->GetTerminalControl();
    }

    // Method Description:
    // - Called after construction of a Tab object to bind event handlers to its
    //   associated Pane and TermControl object and to create the context menu of
    //   the tab item
    // Arguments:
    // - control: reference to the TermControl object to bind event to
    // Return Value:
    // - <none>
    void Tab::Initialize(const TermControl& control)
    {
        _BindEventHandlers(control);
    }

    // Method Description:
    // - Returns true if this is the currently focused tab. For any set of tabs,
    //   there should only be one tab that is marked as focused, though each tab has
    //   no control over the other tabs in the set.
    // Arguments:
    // - <none>
    // Return Value:
    // - true iff this tab is focused.
    bool Tab::IsFocused() const noexcept
    {
        return _focused;
    }

    // Method Description:
    // - Updates our focus state. If we're gaining focus, make sure to transfer
    //   focus to the last focused terminal control in our tree of controls.
    // Arguments:
    // - focused: our new focus state. If true, we should be focused. If false, we
    //   should be unfocused.
    // Return Value:
    // - <none>
    void Tab::SetFocused(const bool focused)
    {
        _focused = focused;

        if (_focused)
        {
            _Focus();
        }
    }

    // Method Description:
    // - Returns nullopt if no children of this tab were the last control to be
    //   focused, or the GUID of the profile of the last control to be focused (if
    //   there was one).
    // Arguments:
    // - <none>
    // Return Value:
    // - nullopt if no children of this tab were the last control to be
    //   focused, else the GUID of the profile of the last control to be focused
    std::optional<GUID> Tab::GetFocusedProfile() const noexcept
    {
        return _activePane->GetFocusedProfile();
    }

    // Method Description:
    // - Called after construction of a Tab object to bind event handlers to its
    //   associated Pane and TermControl object
    // Arguments:
    // - control: reference to the TermControl object to bind event to
    // Return Value:
    // - <none>
    void Tab::_BindEventHandlers(const TermControl& control) noexcept
    {
        _AttachEventHandlersToPane(_rootPane);
        _AttachEventHandlersToControl(control);
    }

    // Method Description:
    // - Attempts to update the settings of this tab's tree of panes.
    // Arguments:
    // - settings: The new TerminalSettings to apply to any matching controls
    // - profile: The GUID of the profile these settings should apply to.
    // Return Value:
    // - <none>
    void Tab::UpdateSettings(const TerminalSettings& settings, const GUID& profile)
    {
        _rootPane->UpdateSettings(settings, profile);
    }

    // Method Description:
    // - Focus the last focused control in our tree of panes.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Tab::_Focus()
    {
        if (!_controlInitialized)
        {
            return;
        }

        _focused = true;

        auto lastFocusedControl = GetActiveTerminalControl();
        if (lastFocusedControl)
        {
            lastFocusedControl.Focus(FocusState::Programmatic);
        }
    }

    // Method Description:
    // - Set the icon on the TabViewItem for this tab.
    // Arguments:
    // - iconPath: The new path string to use as the IconPath for our TabViewItem
    // Return Value:
    // - <none>
    winrt::fire_and_forget Tab::UpdateIcon(const winrt::hstring iconPath)
    {
        // Don't reload our icon if it hasn't changed.
        if (iconPath == _lastIconPath)
        {
            return;
        }

        _lastIconPath = iconPath;

        auto weakThis{ get_weak() };

        auto control = GetActiveTerminalControl();

        co_await winrt::resume_foreground(control.Dispatcher());

        if (auto tab{ weakThis.get() })
        {
            auto newIconSource = GetColoredIcon<winrt::MUX::Controls::BitmapIconSource>(_lastIconPath);
            IconSource(newIconSource);
        }
    }

    // Method Description:
    // - Gets the title string of the last focused terminal control in our tree.
    //   Returns the empty string if there is no such control.
    // Arguments:
    // - <none>
    // Return Value:
    // - the title string of the last focused terminal control in our tree.
    winrt::hstring Tab::GetActiveTitle() const
    {
        if (!_runtimeTabText.empty())
        {
            return _runtimeTabText;
        }
        const auto lastFocusedControl = GetActiveTerminalControl();
        return lastFocusedControl ? lastFocusedControl.Title() : L"";
    }

    // Method Description:
    // - Set the text on the TabViewItem for this tab, and bubbles the new title
    //   value up to anyone listening for changes to our title. Callers can
    //   listen for the title change with a PropertyChanged even handler.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    winrt::fire_and_forget Tab::_UpdateTitle()
    {
        auto weakThis{ get_weak() };

        auto control = GetActiveTerminalControl();

        co_await winrt::resume_foreground(control.Dispatcher());
        if (auto tab{ weakThis.get() })
        {
            // Bubble our current tab text to anyone who's listening for changes.
            Title(GetActiveTitle());

            // Update the UI to reflect the changed
            _UpdateTabHeader();
        }
    }

    // Method Description:
    // - Move the viewport of the terminal up or down a number of lines. Negative
    //      values of `delta` will move the view up, and positive values will move
    //      the viewport down.
    // Arguments:
    // - delta: a number of lines to move the viewport relative to the current viewport.
    // Return Value:
    // - <none>
    winrt::fire_and_forget Tab::Scroll(const int delta)
    {
        auto control = GetActiveTerminalControl();

        co_await winrt::resume_foreground(control.Dispatcher());

        const auto currentOffset = control.GetScrollOffset();
        control.ScrollViewport(currentOffset + delta);
    }

    // Method Description:
    // - Determines whether the focused pane has sufficient space to be split.
    // Arguments:
    // - splitType: The type of split we want to create.
    // Return Value:
    // - True if the focused pane can be split. False otherwise.
    bool Tab::CanSplitPane(winrt::TerminalApp::SplitState splitType)
    {
        return _activePane->CanSplit(splitType);
    }

    // Method Description:
    // - Split the focused pane in our tree of panes, and place the
    //   given TermControl into the newly created pane.
    // Arguments:
    // - splitType: The type of split we want to create.
    // - profile: The profile GUID to associate with the newly created pane.
    // - control: A TermControl to use in the new pane.
    // Return Value:
    // - <none>
    void Tab::SplitPane(winrt::TerminalApp::SplitState splitType, const GUID& profile, TermControl& control)
    {
        auto [first, second] = _activePane->Split(splitType, profile, control);
        _activePane = first;
        _AttachEventHandlersToControl(control);

        // Add a event handlers to the new panes' GotFocus event. When the pane
        // gains focus, we'll mark it as the new active pane.
        _AttachEventHandlersToPane(first);
        _AttachEventHandlersToPane(second);

        // Immediately update our tracker of the focused pane now. If we're
        // splitting panes during startup (from a commandline), then it's
        // possible that the focus events won't propagate immediately. Updating
        // the focus here will give the same effect though.
        _UpdateActivePane(second);
    }

    // Method Description:
    // - See Pane::CalcSnappedDimension
    float Tab::CalcSnappedDimension(const bool widthOrHeight, const float dimension) const
    {
        return _rootPane->CalcSnappedDimension(widthOrHeight, dimension);
    }

    // Method Description:
    // - Update the size of our panes to fill the new given size. This happens when
    //   the window is resized.
    // Arguments:
    // - newSize: the amount of space that the panes have to fill now.
    // Return Value:
    // - <none>
    void Tab::ResizeContent(const winrt::Windows::Foundation::Size& newSize)
    {
        // NOTE: This _must_ be called on the root pane, so that it can propagate
        // throughout the entire tree.
        _rootPane->ResizeContent(newSize);
    }

    // Method Description:
    // - Attempt to move a separator between panes, as to resize each child on
    //   either size of the separator. See Pane::ResizePane for details.
    // Arguments:
    // - direction: The direction to move the separator in.
    // Return Value:
    // - <none>
    void Tab::ResizePane(const winrt::TerminalApp::Direction& direction)
    {
        // NOTE: This _must_ be called on the root pane, so that it can propagate
        // throughout the entire tree.
        _rootPane->ResizePane(direction);
    }

    // Method Description:
    // - Attempt to move focus between panes, as to focus the child on
    //   the other side of the separator. See Pane::NavigateFocus for details.
    // Arguments:
    // - direction: The direction to move the focus in.
    // Return Value:
    // - <none>
    void Tab::NavigateFocus(const winrt::TerminalApp::Direction& direction)
    {
        // NOTE: This _must_ be called on the root pane, so that it can propagate
        // throughout the entire tree.
        _rootPane->NavigateFocus(direction);
    }

    // Method Description:
    // - Prepares this tab for being removed from the UI hierarchy by shutting down all active connections.
    void Tab::Shutdown()
    {
        // TODO: For reasons still unknown, even if a tab is closed and shutdown and removed from TerminalPage's _tabs
        // vector, this current tab's tab color will appear on a new tab. It's almost as if the XAML resource dictionary
        // for the particular TabViewItem this Tab is associated with ends up being reused in a new TabViewItem. Almost like
        // the TabViewItems don't actually get deleted when the corresponding Tab datacontext is removed from the observable
        // vector.
        _ResetTabColor();
        _rootPane->Shutdown();
    }

    // Method Description:
    // - Closes the currently focused pane in this tab. If it's the last pane in
    //   this tab, our Closed event will be fired (at a later time) for anyone
    //   registered as a handler of our close event.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Tab::ClosePane()
    {
        _activePane->Close();
    }

    // Method Description:
    // - Register any event handlers that we may need with the given TermControl.
    //   This should be called on each and every TermControl that we add to the tree
    //   of Panes in this tab. We'll add events too:
    //   * notify us when the control's title changed, so we can update our own
    //     title (if necessary)
    // Arguments:
    // - control: the TermControl to add events to.
    // Return Value:
    // - <none>
    void Tab::_AttachEventHandlersToControl(const TermControl& control)
    {
        auto weakThis{ get_weak() };

        control.TitleChanged([weakThis](auto newTitle) {
            // Check if Tab's lifetime has expired
            if (auto tab{ weakThis.get() })
            {
                // The title of the control changed, but not necessarily the title of the tab.
                // Set the tab's text to the active panes' text.
                tab->_UpdateTitle();
            }
        });

        // This is called when the terminal changes its font size or sets it for the first
        // time (because when we just create terminal via its ctor it has invalid font size).
        // On the latter event, we tell the root pane to resize itself so that its descendants
        // (including ourself) can properly snap to character grids. In future, we may also
        // want to do that on regular font changes.
        control.FontSizeChanged([this](const int /* fontWidth */,
                                       const int /* fontHeight */,
                                       const bool isInitialChange) {
            if (isInitialChange)
            {
                _rootPane->Relayout();
            }
        });

        // Once we know that TermControl has finished its InitializeTerminal steps, we can go
        // ahead and tell this tab to be focused.
        control.TerminalInitialized([weakThis] {
            if (auto tab{ weakThis.get() })
            {
                tab->_controlInitialized = true;
                tab->_Focus();
            }
        });
    }

    // Method Description:
    // - Mark the given pane as the active pane in this tab. All other panes
    //   will be marked as inactive. We'll also update our own UI state to
    //   reflect this newly active pane.
    // Arguments:
    // - pane: a Pane to mark as active.
    // Return Value:
    // - <none>
    void Tab::_UpdateActivePane(std::shared_ptr<Pane> pane)
    {
        // Clear the active state of the entire tree, and mark only the pane as active.
        _rootPane->ClearActive();
        _activePane = pane;
        _activePane->SetActive();

        // Update our own title text to match the newly-active pane.
        _UpdateTitle();

        // Raise our own ActivePaneChanged event.
        _ActivePaneChangedHandlers();
    }

    // Method Description:
    // - Add an event handler to this pane's GotFocus event. When that pane gains
    //   focus, we'll mark it as the new active pane. We'll also query the title of
    //   that pane when it's focused to set our own text, and finally, we'll trigger
    //   our own ActivePaneChanged event.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Tab::_AttachEventHandlersToPane(std::shared_ptr<Pane> pane)
    {
        auto weakThis{ get_weak() };

        pane->GotFocus([weakThis](std::shared_ptr<Pane> sender) {
            // Do nothing if the Tab's lifetime is expired or pane isn't new.
            auto tab{ weakThis.get() };

            if (tab && sender != tab->_activePane)
            {
                tab->_UpdateActivePane(sender);
            }
        });
    }

    void Tab::_OnTabItemLoaded(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& /*e*/)
    {
        if (auto temp = sender.try_as<winrt::Microsoft::UI::Xaml::Controls::TabViewItem>())
        {
            _tabViewItem = temp;
            _CreateContextMenu();
        }
    }

    void Tab::_OnCloseTabMenuItemClick(const IInspectable& /*sender*/, const Windows::UI::Xaml::RoutedEventArgs& /*e*/)
    {
        _rootPane->Close();
    }

    void Tab::_OnColorMenuItemClick(const IInspectable& /*sender*/, const Windows::UI::Xaml::RoutedEventArgs& /*e*/)
    {
        _tabColorPickup.ShowAt(_tabViewItem);
    }

    void Tab::_OnRenameTabMenuItemClick(const IInspectable& /*sender*/, const Windows::UI::Xaml::RoutedEventArgs& /*e*/)
    {
        _inRename = true;
        _UpdateTabHeader();
    }

    // Method Description:
    // - Creates a context menu attached to the tab.
    // Currently contains elements allowing to select or
    // to close the current tab
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Tab::_CreateContextMenu()
    {
        auto weakThis{ get_weak() };

        // Color Picker (it's convenient to have it here)
        _tabColorPickup.ColorSelected([weakThis](auto newTabColor) {
            if (auto tab{ weakThis.get() })
            {
                tab->_SetTabColor(newTabColor);
            }
        });

        _tabColorPickup.ColorCleared([weakThis]() {
            if (auto tab{ weakThis.get() })
            {
                tab->_ResetTabColor();
            }
        });
    }

    // Method Description:
    // - This will update the contents of our TabViewItem for our current state.
    //   - If we're not in a rename, we'll set the Header of the TabViewItem to
    //     simply our current tab text (either the runtime tab text or the
    //     active terminal's text).
    //   - If we're in a rename, then we'll set the Header to a TextBox with the
    //     current tab text. The user can then use that TextBox to set a string
    //     to use as an override for the tab's text.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Tab::_UpdateTabHeader()
    {
        winrt::hstring tabText{ GetActiveTitle() };

        if (!_inRename)
        {
            // If we're not currently in the process of renaming the tab, then just set the tab's text to whatever our active title is.
            Title(tabText);
        }
        else
        {
            _ConstructTabRenameBox(tabText);
        }
    }

    // Method Description:
    // - Create a new TextBox to use as the control for renaming the tab text.
    //   If the text box is already created, then this will do nothing, and
    //   leave the current box unmodified.
    // Arguments:
    // - tabText: This should be the text to initialize the rename text box with.
    // Return Value:
    // - <none>
    void Tab::_ConstructTabRenameBox(const winrt::hstring& tabText)
    {
        if (_tabViewItem.Header().try_as<Controls::TextBox>())
        {
            return;
        }

        Controls::TextBox tabTextBox;
        tabTextBox.Text(tabText);

        // The TextBox has a MinHeight already set by default, which is
        // larger than we want. Get rid of it.
        tabTextBox.MinHeight(0);
        // Also get rid of the internal padding on the text box, between the
        // border and the text content, on the top and bottom. This will
        // help the box fit within the bounds of the tab.
        Thickness internalPadding = ThicknessHelper::FromLengths(4, 0, 4, 0);
        tabTextBox.Padding(internalPadding);

        // Make the margin (0, -8, 0, -8), to counteract the padding that
        // the TabViewItem has.
        //
        // This is maybe a bit fragile, as the actual value might not be exactly
        // (0, 8, 0, 8), but using TabViewItemHeaderPadding to look up the real
        // value at runtime didn't work. So this is good enough for now.
        Thickness negativeMargins = ThicknessHelper::FromLengths(0, -8, 0, -8);
        tabTextBox.Margin(negativeMargins);

        // Set up some event handlers on the text box. We need three of them:
        // * A LostFocus event, so when the TextBox loses focus, we'll
        //   remove it and return to just the text on the tab.
        // * A KeyUp event, to be able to submit the tab text on Enter or
        //   dismiss the text box on Escape
        // * A LayoutUpdated event, so that we can auto-focus the text box
        //   when it's added to the tree.
        auto weakThis{ get_weak() };

        // When the text box loses focus, update the tab title of our tab.
        // - If there are any contents in the box, we'll use that value as
        //   the new "runtime text", which will override any text set by the
        //   application.
        // - If the text box is empty, we'll reset the "runtime text", and
        //   return to using the active terminal's title.
        tabTextBox.LostFocus([weakThis](const IInspectable& sender, auto&&) {
            auto tab{ weakThis.get() };
            auto textBox{ sender.try_as<Controls::TextBox>() };
            if (tab && textBox)
            {
                tab->_runtimeTabText = textBox.Text();
                tab->_inRename = false;
                tab->_UpdateTitle();
            }
        });

        // NOTE: (Preview)KeyDown does not work here. If you use that, we'll
        // remove the TextBox from the UI tree, then the following KeyUp
        // will bubble to the NewTabButton, which we don't want to have
        // happen.
        tabTextBox.KeyUp([weakThis](const IInspectable& sender, Input::KeyRoutedEventArgs const& e) {
            auto tab{ weakThis.get() };
            auto textBox{ sender.try_as<Controls::TextBox>() };
            if (tab && textBox)
            {
                switch (e.OriginalKey())
                {
                case VirtualKey::Enter:
                    tab->_runtimeTabText = textBox.Text();
                    [[fallthrough]];
                case VirtualKey::Escape:
                    e.Handled(true);
                    textBox.Text(tab->_runtimeTabText);
                    tab->_inRename = false;
                    tab->_UpdateTitle();
                    break;
                }
            }
        });

        // As soon as the text box is added to the UI tree, focus it. We can't focus it till it's in the tree.
        _tabRenameBoxLayoutUpdatedRevoker = tabTextBox.LayoutUpdated(winrt::auto_revoke, [this](auto&&, auto&&) {
            // Curiously, the sender for this event is null, so we have to
            // get the TextBox from the Tab's Header().
            auto textBox{ _tabViewItem.Header().try_as<Controls::TextBox>() };
            if (textBox)
            {
                textBox.SelectAll();
                textBox.Focus(FocusState::Programmatic);
            }
            // Only let this succeed once.
            _tabRenameBoxLayoutUpdatedRevoker.revoke();
        });

        _tabViewItem.Header(tabTextBox);
    }

    // Method Description:
    // Returns the tab color, if any
    // Arguments:
    // - <none>
    // Return Value:
    // - The tab's color, if any
    std::optional<winrt::Windows::UI::Color> Tab::GetTabColor()
    {
        return _tabColor;
    }

    // Method Description:
    // - Sets the tab background color to the color chosen by the user
    // - Sets the tab foreground color depending on the luminance of
    // the background color
    // Arguments:
    // - color: the shiny color the user picked for their tab
    // Return Value:
    // - <none>
    void Tab::_SetTabColor(const winrt::Windows::UI::Color& color)
    {
        auto weakThis{ get_weak() };

        _tabViewItem.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [weakThis, color]() {
            auto ptrTab = weakThis.get();
            if (!ptrTab)
                return;

            auto tab{ ptrTab };
            Media::SolidColorBrush selectedTabBrush{};
            Media::SolidColorBrush deselectedTabBrush{};
            Media::SolidColorBrush fontBrush{};
            Media::SolidColorBrush hoverTabBrush{};
            // calculate the luminance of the current color and select a font
            // color based on that
            // see https://www.w3.org/TR/WCAG20/#relativeluminancedef
            if (TerminalApp::ColorHelper::IsBrightColor(color))
            {
                fontBrush.Color(winrt::Windows::UI::Colors::Black());
            }
            else
            {
                fontBrush.Color(winrt::Windows::UI::Colors::White());
            }

            hoverTabBrush.Color(TerminalApp::ColorHelper::GetAccentColor(color));
            selectedTabBrush.Color(color);

            // currently if a tab has a custom color, a deselected state is
            // signified by using the same color with a bit ot transparency
            auto deselectedTabColor = color;
            deselectedTabColor.A = 64;
            deselectedTabBrush.Color(deselectedTabColor);

            // currently if a tab has a custom color, a deselected state is
            // signified by using the same color with a bit ot transparency
            tab->_tabViewItem.Resources().Insert(winrt::box_value(L"TabViewItemHeaderBackgroundSelected"), selectedTabBrush);
            tab->_tabViewItem.Resources().Insert(winrt::box_value(L"TabViewItemHeaderBackground"), deselectedTabBrush);
            tab->_tabViewItem.Resources().Insert(winrt::box_value(L"TabViewItemHeaderBackgroundPointerOver"), hoverTabBrush);
            tab->_tabViewItem.Resources().Insert(winrt::box_value(L"TabViewItemHeaderBackgroundPressed"), selectedTabBrush);
            tab->_tabViewItem.Resources().Insert(winrt::box_value(L"TabViewItemHeaderForeground"), fontBrush);
            tab->_tabViewItem.Resources().Insert(winrt::box_value(L"TabViewItemHeaderForegroundSelected"), fontBrush);
            tab->_tabViewItem.Resources().Insert(winrt::box_value(L"TabViewItemHeaderForegroundPointerOver"), fontBrush);
            tab->_tabViewItem.Resources().Insert(winrt::box_value(L"TabViewItemHeaderForegroundPressed"), fontBrush);
            tab->_tabViewItem.Resources().Insert(winrt::box_value(L"TabViewButtonForegroundActiveTab"), fontBrush);

            tab->_RefreshVisualState();

            tab->_tabColor.emplace(color);
            tab->_colorSelected(color);
        });
    }

    // Method Description:
    // Clear the custom color of the tab, if any
    // the background color
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Tab::_ResetTabColor()
    {
        auto weakThis{ get_weak() };

        auto ptrTab = weakThis.get();
        if (!ptrTab)
            return;

        auto tab{ ptrTab };
        winrt::hstring keys[] = {
            L"TabViewItemHeaderBackground",
            L"TabViewItemHeaderBackgroundSelected",
            L"TabViewItemHeaderBackgroundPointerOver",
            L"TabViewItemHeaderForeground",
            L"TabViewItemHeaderForegroundSelected",
            L"TabViewItemHeaderForegroundPointerOver",
            L"TabViewItemHeaderBackgroundPressed",
            L"TabViewItemHeaderForegroundPressed",
            L"TabViewButtonForegroundActiveTab"
        };

        // simply clear any of the colors in the tab's dict
        for (auto keyString : keys)
        {
            auto key = winrt::box_value(keyString);
            if (tab->_tabViewItem.Resources().HasKey(key))
            {
                tab->_tabViewItem.Resources().Remove(key);
            }
        }

        tab->_RefreshVisualState();
        tab->_tabColor.reset();
        tab->_colorCleared();
    }

    // Method Description:
    // Toggles the visual state of the tab view item,
    // so that changes to the tab color are reflected immediately
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Tab::_RefreshVisualState()
    {
        if (_focused)
        {
            VisualStateManager::GoToState(_tabViewItem, L"Normal", true);
            VisualStateManager::GoToState(_tabViewItem, L"Selected", true);
        }
        else
        {
            VisualStateManager::GoToState(_tabViewItem, L"Selected", true);
            VisualStateManager::GoToState(_tabViewItem, L"Normal", true);
        }
    }

    // - Get the total number of leaf panes in this tab. This will be the number
    //   of actual controls hosted by this tab.
    // Arguments:
    // - <none>
    // Return Value:
    // - The total number of leaf panes hosted by this tab.
    int Tab::_GetLeafPaneCount() const noexcept
    {
        return _rootPane->GetLeafPaneCount();
    }

    // Method Description:
    // - This is a helper to determine which direction an "Automatic" split should
    //   happen in for the active pane of this tab, but without using the ActualWidth() and
    //   ActualHeight() methods.
    // - See Pane::PreCalculateAutoSplit
    // Arguments:
    // - availableSpace: The theoretical space that's available for this Tab's content
    // Return Value:
    // - The SplitState that we should use for an `Automatic` split given
    //   `availableSpace`
    SplitState Tab::PreCalculateAutoSplit(winrt::Windows::Foundation::Size availableSpace) const
    {
        return _rootPane->PreCalculateAutoSplit(_activePane, availableSpace).value_or(SplitState::Vertical);
    }

    DEFINE_EVENT(Tab, ActivePaneChanged, _ActivePaneChangedHandlers, winrt::delegate<>);
    DEFINE_EVENT(Tab, ColorSelected, _colorSelected, winrt::delegate<winrt::Windows::UI::Color>);
    DEFINE_EVENT(Tab, ColorCleared, _colorCleared, winrt::delegate<>);
}
